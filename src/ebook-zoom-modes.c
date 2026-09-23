// Copyright (C) 2026 Mitchell Scott
// SPDX-License-Identifier: GPL-3.0-only

#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "xovi.h"

#define LOG_PREFIX "[ebook-zoom-modes]"

// SceneView applies the document zoom mode only when document.fileType == Pdf (2), in three places:
//   zoom mode calculator: any other type gets zoomMode = defaultZoomMode = FitToWidth
//   default focal point:  Ebook (3) takes its own fit-the-page branch instead of the per-mode one
//   updatePolish:         any other type skips the per-mode refit after a resize or rotation
// Ebook is the largest FileType, so widening each test to fileType >= Pdf admits ebooks and nothing else
struct MaskedInstruction {
    uint32_t value;
    uint32_t mask;
};

#define LOAD_FILE_TYPE {0xb9422800, 0xfffffc00}          // ldr  wT, [xN, #0x228]
#define COMPARE_WITH_PDF {0x7100081f, 0xfffffc1f}        // cmp  wN, #2
#define COMPARE_WITH_EBOOK {0x71000c1f, 0xfffffc1f}      // cmp  wN, #3
#define BRANCH_IF_EQUAL {0x54000000, 0xff00001f}         // b.eq
#define BRANCH_IF_NOT_EQUAL {0x54000001, 0xff00001f}     // b.ne
#define ANY_CONDITIONAL_BRANCH {0x54000000, 0xff000010}  // b.cond
#define MOVE_ONE {0x52800020, 0xffffffe0}                // mov  wD, #1
#define LOAD_DOCUMENT {0xf940cc00, 0xfffffc00}           // ldr  xT, [xN, #0x198]
#define LOAD_CONTENT_RECT {0xf9408400, 0xfffffc00}       // ldr  xT, [xN, #0x108]

#define CONDITION_GREATER_OR_EQUAL 0xa
#define CONDITION_LESS_THAN 0xb

static const struct MaskedInstruction zoomModeCalculatorGate[] = {
    LOAD_FILE_TYPE, COMPARE_WITH_PDF, BRANCH_IF_EQUAL, MOVE_ONE,
};
static const struct MaskedInstruction defaultFocalPointGate[] = {
    LOAD_FILE_TYPE, COMPARE_WITH_PDF, BRANCH_IF_EQUAL, COMPARE_WITH_EBOOK, ANY_CONDITIONAL_BRANCH,
};
static const struct MaskedInstruction updatePolishGate[] = {
    LOAD_DOCUMENT, LOAD_FILE_TYPE, COMPARE_WITH_PDF, BRANCH_IF_NOT_EQUAL, LOAD_CONTENT_RECT,
};

struct FileTypeGate {
    const char *name;
    const struct MaskedInstruction *signature;
    size_t signatureLength;
    size_t branchIndex;
    uint32_t condition;
};

#define GATE(name, signature, branchIndex, condition) \
    {name, signature, sizeof(signature) / sizeof(signature[0]), branchIndex, condition}

static const struct FileTypeGate fileTypeGates[] = {
    GATE("zoom mode calculator", zoomModeCalculatorGate, 2, CONDITION_GREATER_OR_EQUAL),
    GATE("default focal point", defaultFocalPointGate, 2, CONDITION_GREATER_OR_EQUAL),
    GATE("updatePolish", updatePolishGate, 3, CONDITION_LESS_THAN),
};
#define FILE_TYPE_GATE_COUNT (sizeof(fileTypeGates) / sizeof(fileTypeGates[0]))

struct CodeRange {
    uint8_t *start;
    size_t length;
};

static int isXochitlProcess(void)
{
    char executable[64] = {0};
    ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    return length > 0 && strcmp(executable, "/usr/bin/xochitl") == 0;
}

static int findMainProgramCode(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)size;
    struct CodeRange *code = data;
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *header = &info->dlpi_phdr[i];
        if (header->p_type == PT_LOAD && (header->p_flags & PF_X)) {
            code->start = (uint8_t *)(info->dlpi_addr + header->p_vaddr);
            code->length = header->p_memsz;
        }
    }
    return 1;
}

static int matchesAt(const uint32_t *code, const struct FileTypeGate *gate)
{
    for (size_t i = 0; i < gate->signatureLength; i++) {
        if ((code[i] & gate->signature[i].mask) != gate->signature[i].value)
            return 0;
    }
    return 1;
}

static uint32_t *findUniqueGate(const struct CodeRange *code, const struct FileTypeGate *gate, int *matchCount)
{
    const uint32_t *instructions = (const uint32_t *)code->start;
    size_t instructionCount = code->length / 4;
    uint32_t *match = NULL;
    *matchCount = 0;
    for (size_t i = 0; i + gate->signatureLength <= instructionCount; i++) {
        if (matchesAt(instructions + i, gate)) {
            match = (uint32_t *)(instructions + i);
            (*matchCount)++;
        }
    }
    return *matchCount == 1 ? match : NULL;
}

static uint32_t withCondition(uint32_t conditionalBranch, uint32_t condition)
{
    return (conditionalBranch & ~0xfu) | condition;
}

static int writeInstruction(uint32_t *location, uint32_t instruction)
{
    uintptr_t pageSize = (uintptr_t)sysconf(_SC_PAGESIZE);
    void *page = (void *)((uintptr_t)location & ~(pageSize - 1));
    if (mprotect(page, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
        return 0;
    *location = instruction;
    mprotect(page, pageSize, PROT_READ | PROT_EXEC);
    __builtin___clear_cache((char *)location, (char *)location + sizeof(instruction));
    return 1;
}

static int applyZoomModesToEbooks(const struct CodeRange *code)
{
    uint32_t *branches[FILE_TYPE_GATE_COUNT];
    for (size_t i = 0; i < FILE_TYPE_GATE_COUNT; i++) {
        int matchCount;
        uint32_t *gate = findUniqueGate(code, &fileTypeGates[i], &matchCount);
        if (!gate) {
            fprintf(stderr, LOG_PREFIX " %s gate found %d times, unsupported xochitl build, staying inactive\n",
                    fileTypeGates[i].name, matchCount);
            return 0;
        }
        branches[i] = gate + fileTypeGates[i].branchIndex;
    }

    for (size_t i = 0; i < FILE_TYPE_GATE_COUNT; i++) {
        if (!writeInstruction(branches[i], withCondition(*branches[i], fileTypeGates[i].condition))) {
            fprintf(stderr, LOG_PREFIX " could not make xochitl code writable at the %s gate\n", fileTypeGates[i].name);
            return 0;
        }
    }
    fprintf(stderr, LOG_PREFIX " ebooks now follow the document zoom mode\n");
    return 1;
}

void _xovi_construct(void)
{
    Environment->requireExtension("qt-resource-rebuilder", 0, 2, 0);
    if (!isXochitlProcess())
        return;

    struct CodeRange code = {0};
    dl_iterate_phdr(findMainProgramCode, &code);
    if (!code.start) {
        fprintf(stderr, LOG_PREFIX " could not locate xochitl code, staying inactive\n");
        return;
    }

    if (applyZoomModesToEbooks(&code))
        qt_resource_rebuilder$qmldiff_add_external_diff(r$ebookZoomModes, "Adjust View on ebooks");
}
