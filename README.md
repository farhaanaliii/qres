<div align="center">

# qres

[![PyPI](https://img.shields.io/pypi/v/qres)](https://pypi.org/project/qres)
[![Python](https://img.shields.io/pypi/pyversions/qres)](https://pypi.org/project/qres)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Build](https://img.shields.io/github/actions/workflow/status/farhaanaliii/qres/ci.yml?branch=main)](https://github.com/farhaanaliii/qres/actions)

A fast Qt resource compiler for Python, implemented as a C extension. Drop-in replacement for `pyrcc5` / `pyside6-rcc` that compiles `.qrc` files into `_rc.py` modules without requiring a Qt installation.

</div>

## Installation

```
pip install qres
```

Pre-built wheels are available for Linux (x86_64, aarch64), macOS (x86_64, arm64), and Windows (AMD64, ARM64) on Python 3.11+.

## Usage

**CLI**

```
qres resources.qrc
qres resources.qrc -o path/to/resources_rc.py
qres resources.qrc --binding PySide6
```

**Python API**

```python
import qres

# Compile from a file path
qres.compile_file("resources.qrc")
qres.compile_file("resources.qrc", output="resources_rc.py")
qres.compile_file("resources.qrc", binding="PySide6")

# Compile from an XML string, returns raw blobs
blobs = qres.compile(xml_string, base_dir="/path/to/qrc/dir")
# blobs: {"data": bytes, "name": bytes, "struct_v1": bytes, "struct_v2": bytes}
```

The generated `_rc.py` works with `qtpy`, `PyQt5`, `PyQt6`, `PySide2`, and `PySide6`. By default, it includes an automatic fallback import chain that resolves whichever Qt binding is installed in the active environment.

## How it works

qres parses the `.qrc` XML, builds a virtual file tree sorted by Qt's hash function, compresses each asset with zlib at level 9 (skipping compression when it doesn't help or for `.ico` files), then serializes two binary struct formats — `v1` for Qt < 5.8 and `v2` for Qt >= 5.8 — into Python byte literals.

## Benchmarks

Benchmarked on a 50 MB uncompressed text asset compiled into a registered Qt resource module:

| Metric | Measurement |
|---|---|
| Input payload | 50.00 MB (52,428,800 bytes) |
| Compile duration | 403 ms (0.40 s) |
| Compilation throughput | 124 MB/s |
| Generated module size | 1.23 MB (97.5% compression) |
| Qt resource read speed | 93 ms (534 MB/s) |
| Data integrity | SHA-256 verified |


## Contributing

See [CONTRIBUTING.md](.github/CONTRIBUTING.md).

## Building from source

Requires a C compiler.

```
git clone https://github.com/farhaanaliii/qres
cd qres
pip install -e .
```

## License

MIT — see [LICENSE](LICENSE).
