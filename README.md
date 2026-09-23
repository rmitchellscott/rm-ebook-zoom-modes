# ebook-zoom-modes

[![rmpp](https://img.shields.io/badge/rMPP-supported-green)](https://remarkable.com/store/overview/remarkable-paper-pro)
[![rmppmove](https://img.shields.io/badge/rMPPMove-supported-green)](https://remarkable.com/products/remarkable-paper/pro-move)
[![rmppure](https://img.shields.io/badge/rMPPure-supported-green)](https://remarkable.com/products/remarkable-paper/pure)

A xovi extension that lets ebooks use the Adjust View zoom modes: fit to width, fit to height and custom fit.

xochitl applies a document's zoom mode only to PDFs. An ebook always opens at a fixed fit, and a zoom mode set on it has no effect. This extension sends ebooks through the same zoom code that PDFs use, so the chosen mode is applied, saved with the book, and kept across page turns and rotation.

## Dependencies

- [xovi](https://github.com/asivery/rm-xovi-extensions) - Extension framework
    - qt-resource-rebuilder - Required to show the Adjust View menu on ebooks

## Installation

### Manual

1. Ensure xovi is installed
2. Download `ebook-zoom-modes.so` from the [latest release](https://github.com/rmitchellscott/rm-ebook-zoom-modes/releases/latest) and place it in `/home/root/xovi/extensions.d/`
3. Restart xovi

The QML patch that shows the Adjust View menu on ebooks is built into the extension. 

## License

Copyright (C) 2026 Mitchell Scott

Licensed under the GNU General Public License v3.0.
