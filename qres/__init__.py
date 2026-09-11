import argparse
from pathlib import Path
import sys

from ._qres import compile as _compile

_BINDINGS = ("PyQt6", "PySide6", "PyQt5", "PySide2", "qtpy")

_HEADER_BOILERPLATE = """# -*- coding: utf-8 -*-

# Resource object code
#
# Created by: qres
#
# WARNING! All changes made in this file will be lost!

"""

_FOOTER_BOILERPLATE = """qt_version = [int(v) for v in QtCore.qVersion().split('.')]
if qt_version < [5, 8, 0]:
    rcc_version = 1
    qt_resource_struct = qt_resource_struct_v1
else:
    rcc_version = 2
    qt_resource_struct = qt_resource_struct_v2

def qInitResources():
    QtCore.qRegisterResourceData(rcc_version, qt_resource_struct, qt_resource_name, qt_resource_data)

def qCleanupResources():
    QtCore.qUnregisterResourceData(rcc_version, qt_resource_struct, qt_resource_name, qt_resource_data)

qInitResources()
"""

_RESOURCE_BLOBS = (
    ("qt_resource_data", "data"),
    ("qt_resource_name", "name"),
    ("qt_resource_struct_v1", "struct_v1"),
    ("qt_resource_struct_v2", "struct_v2"),
)

_BYTE_ESCAPES = tuple(f"\\x{byte:02x}" for byte in range(256))


def compile(xml: str, base_dir: str) -> dict[str, bytes]:
    return _compile(xml, base_dir)

def _generate_import_header(binding: str) -> str:
    if binding != "auto":
        return f"from {binding} import QtCore\n\n"

    lines = []
    for i, name in enumerate(_BINDINGS[:-1]):
        indent = "    " * i
        lines.append(f"{indent}try:\n{indent}    from {name} import QtCore\n{indent}except ImportError:\n")

    indent = "    " * (len(_BINDINGS) - 1)
    lines.append(f"{indent}from {_BINDINGS[-1]} import QtCore\n\n")
    return "".join(lines)

def _generate_resource_blob(name: str, data: bytes) -> str:
    return (
        f'{name} = b"\\\n'
        + "".join(
            "".join(_BYTE_ESCAPES[byte] for byte in data[i:i + 16]) + "\\\n"
            for i in range(0, len(data), 16)
        )
        + '"\n\n'
    )


def compile_file(
    qrc_path: str | Path,
    output: str | Path | None = None,
    binding: str = "auto",
) -> None:
    qrc_path = Path(qrc_path)
    blobs = compile(qrc_path.read_text(encoding="utf-8"), str(qrc_path.parent))

    target = Path(output) if output else qrc_path.with_name(f"{qrc_path.stem}_rc.py")
    import_header = _generate_import_header(binding)
    blobs_code = "".join(_generate_resource_blob(var_name, blobs[key]) for var_name, key in _RESOURCE_BLOBS)

    target.write_text(_HEADER_BOILERPLATE + import_header + blobs_code + _FOOTER_BOILERPLATE, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(
        prog="qres",
        description="Compile a .qrc file into a Python resource module.",
    )
    parser.add_argument("qrc", help="Path to the .qrc file")
    parser.add_argument("-o", "--output", default=None, help="Output .py file path")
    parser.add_argument(
        "--binding",
        choices=("auto", *_BINDINGS),
        default="auto",
        help="Target Qt binding (default: auto)",
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Print verbose output")
    args = parser.parse_args()

    try:
        compile_file(args.qrc, args.output, binding=args.binding)
        if args.verbose:
            print(f"Resource compilation completed successfully: {args.qrc}")
    except Exception as exc:
        sys.stderr.write(f"qres: error: {exc}\n")
        sys.exit(1)
