from pathlib import Path
import pytest
import qres


def test_compile_basic(tmp_path: Path) -> None:
    asset = tmp_path / "hello.txt"
    asset.write_text("Hello Qt Resources!", encoding="utf-8")

    qrc = f"""<RCC>
    <qresource>
        <file>{asset.name}</file>
    </qresource>
</RCC>"""

    blobs = qres.compile(qrc, str(tmp_path))
    assert "data" in blobs
    assert "name" in blobs
    assert "struct_v1" in blobs
    assert "struct_v2" in blobs
    assert len(blobs["data"]) > 0
    assert len(blobs["name"]) > 0


def test_compile_with_alias(tmp_path: Path) -> None:
    asset = tmp_path / "actual_file.txt"
    asset.write_text("Aliased Content", encoding="utf-8")

    qrc = f"""<RCC>
    <qresource>
        <file alias="virtual_alias.txt">{asset.name}</file>
    </qresource>
</RCC>"""

    blobs = qres.compile(qrc, str(tmp_path))
    assert len(blobs["data"]) > 0
    # In Qt name table, "virtual_alias.txt" is encoded in UTF-16BE
    alias_bytes = "virtual_alias.txt".encode("utf-16-be")
    assert alias_bytes in blobs["name"]


def test_compile_with_prefix(tmp_path: Path) -> None:
    asset = tmp_path / "icon.png"
    asset.write_bytes(b"\x89PNG\r\n\x1a\nfake_png_data")

    qrc = f"""<RCC>
    <qresource prefix="/icons/toolbar">
        <file>{asset.name}</file>
    </qresource>
</RCC>"""

    blobs = qres.compile(qrc, str(tmp_path))
    assert len(blobs["data"]) > 0
    icons_bytes = "icons".encode("utf-16-be")
    toolbar_bytes = "toolbar".encode("utf-16-be")
    icon_name_bytes = "icon.png".encode("utf-16-be")
    assert icons_bytes in blobs["name"]
    assert toolbar_bytes in blobs["name"]
    assert icon_name_bytes in blobs["name"]


def test_compile_ignores_comments(tmp_path: Path) -> None:
    asset = tmp_path / "kept.txt"
    asset.write_text("Keep me", encoding="utf-8")

    qrc = f"""<RCC>
    <!-- This is a comment: <file>missing.txt</file> -->
    <qresource>
        <file>{asset.name}</file>
    </qresource>
</RCC>"""

    blobs = qres.compile(qrc, str(tmp_path))
    assert len(blobs["data"]) > 0
    missing_bytes = "missing.txt".encode("utf-16-be")
    assert missing_bytes not in blobs["name"]


def test_compile_utf8_filename(tmp_path: Path) -> None:
    asset = tmp_path / "café_res.txt"
    asset.write_text("Coffee", encoding="utf-8")

    qrc = f"""<RCC>
    <qresource>
        <file>{asset.name}</file>
    </qresource>
</RCC>"""

    blobs = qres.compile(qrc, str(tmp_path))
    expected_utf16be = asset.name.encode("utf-16-be")
    assert expected_utf16be in blobs["name"]


def test_compile_malformed_xml_raises(tmp_path: Path) -> None:
    qrc = "<RCC><qresource><file>unclosed"
    with pytest.raises(RuntimeError, match="XML syntax error"):
        qres.compile(qrc, str(tmp_path))


def test_compile_missing_file_raises(tmp_path: Path) -> None:
    qrc = """<RCC>
    <qresource>
        <file>non_existent_asset.txt</file>
    </qresource>
</RCC>"""
    with pytest.raises(RuntimeError, match="Cannot open file"):
        qres.compile(qrc, str(tmp_path))


def test_compile_file_and_binding(tmp_path: Path) -> None:
    asset = tmp_path / "data.bin"
    asset.write_bytes(b"\x00\x01\x02\x03")

    qrc_file = tmp_path / "test.qrc"
    qrc_file.write_text(
        f"""<RCC>
    <qresource prefix="assets">
        <file>{asset.name}</file>
    </qresource>
</RCC>""",
        encoding="utf-8",
    )

    out_file = tmp_path / "test_rc.py"
    qres.compile_file(qrc_file, out_file, binding="PySide6")

    assert out_file.exists()
    content = out_file.read_text(encoding="utf-8")
    assert "from PySide6 import QtCore" in content
    assert "qt_resource_data = b\"" in content
    assert "qt_resource_name = b\"" in content
    assert "qt_resource_struct_v1 = b\"" in content
    assert "qt_resource_struct_v2 = b\"" in content
    assert "qInitResources()" in content
    assert "qCleanupResources()" in content


def test_compile_file_default_auto_binding(tmp_path: Path) -> None:
    asset = tmp_path / "data.bin"
    asset.write_bytes(b"\x00\x01")

    qrc_file = tmp_path / "test.qrc"
    qrc_file.write_text(
        f"""<RCC>
    <qresource>
        <file>{asset.name}</file>
    </qresource>
</RCC>""",
        encoding="utf-8",
    )

    out_file = tmp_path / "auto_rc.py"
    qres.compile_file(qrc_file, out_file)

    assert out_file.exists()
    content = out_file.read_text(encoding="utf-8")
    assert "except ImportError:" in content
    assert "from PyQt6 import QtCore" in content
    assert "from qtpy import QtCore" in content

