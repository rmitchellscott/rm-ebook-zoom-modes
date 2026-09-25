XOVI_REPO ?= $(HOME)/github/asivery/xovi
name = ebook-zoom-modes

VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")

XOVI_VERSION = $(shell echo "$(VERSION)" | sed 's/^v//' | awk -F'[.-]' '{if (NF >= 3 && $$1 ~ /^[0-9]+$$/) printf "%d", ($$1 * 65536) + ($$2 * 256) + $$3; else print "0"}')

CC_AARCH64 ?= aarch64-linux-gnu-gcc
CC_ARMV7 ?= arm-linux-gnueabihf-gcc
CFLAGS = -std=gnu17 -D_GNU_SOURCE -fPIC -O2 -Wall -Wextra -DVERSION=\"$(VERSION)\"
LDFLAGS =

BUILD_AARCH64 = build-aarch64
BUILD_ARMV7 = build-armv7

OBJECTS_AARCH64 = $(BUILD_AARCH64)/ebook-zoom-modes.o $(BUILD_AARCH64)/xovi.o
OBJECTS_ARMV7 = $(BUILD_ARMV7)/ebook-zoom-modes.o $(BUILD_ARMV7)/xovi.o

.PHONY: all clean clean-aarch64 clean-armv7 print-version

print-version:
	@echo "VERSION=$(VERSION)"
	@echo "XOVI_VERSION=$(XOVI_VERSION)"

all: $(name)-aarch64.so $(name)-armv7.so

# ARM64 (aarch64) build
$(name)-aarch64.so: $(OBJECTS_AARCH64)
	$(CC_AARCH64) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS)

$(BUILD_AARCH64)/%.o: src/%.c $(BUILD_AARCH64)/xovi.h
	$(CC_AARCH64) $(CFLAGS) -I$(BUILD_AARCH64) -c $< -o $@

$(BUILD_AARCH64)/xovi.o: $(BUILD_AARCH64)/xovi.c $(BUILD_AARCH64)/xovi.h
	$(CC_AARCH64) $(CFLAGS) -I$(BUILD_AARCH64) -c $< -o $@

$(BUILD_AARCH64)/xovi.h: $(BUILD_AARCH64)/xovi.c
$(BUILD_AARCH64)/xovi.c: $(name).xovi qmd/$(name).qmd
	@mkdir -p $(BUILD_AARCH64)
	python3 $(XOVI_REPO)/util/xovigen.py -a aarch64 -o $(BUILD_AARCH64)/xovi.c -H $(BUILD_AARCH64)/xovi.h $(name).xovi
	@sed -i.bak 's/const int EXTENSIONVERSION = [0-9]*/const int EXTENSIONVERSION = $(XOVI_VERSION)/' $(BUILD_AARCH64)/xovi.c && rm -f $(BUILD_AARCH64)/xovi.c.bak

# ARM32v7 (armv7) build
$(name)-armv7.so: $(OBJECTS_ARMV7)
	$(CC_ARMV7) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS)

$(BUILD_ARMV7)/%.o: src/%.c $(BUILD_ARMV7)/xovi.h
	$(CC_ARMV7) $(CFLAGS) -I$(BUILD_ARMV7) -c $< -o $@

$(BUILD_ARMV7)/xovi.o: $(BUILD_ARMV7)/xovi.c $(BUILD_ARMV7)/xovi.h
	$(CC_ARMV7) $(CFLAGS) -I$(BUILD_ARMV7) -c $< -o $@

$(BUILD_ARMV7)/xovi.h: $(BUILD_ARMV7)/xovi.c
$(BUILD_ARMV7)/xovi.c: $(name).xovi qmd/$(name).qmd
	@mkdir -p $(BUILD_ARMV7)
	python3 $(XOVI_REPO)/util/xovigen.py -a arm32 -o $(BUILD_ARMV7)/xovi.c -H $(BUILD_ARMV7)/xovi.h $(name).xovi
	@sed -i.bak 's/const int EXTENSIONVERSION = [0-9]*/const int EXTENSIONVERSION = $(XOVI_VERSION)/' $(BUILD_ARMV7)/xovi.c && rm -f $(BUILD_ARMV7)/xovi.c.bak

clean-aarch64:
	rm -f $(name)-aarch64.so $(BUILD_AARCH64)/*.o

clean-armv7:
	rm -f $(name)-armv7.so $(BUILD_ARMV7)/*.o

clean: clean-aarch64 clean-armv7
	rm -rf $(BUILD_AARCH64) $(BUILD_ARMV7)
