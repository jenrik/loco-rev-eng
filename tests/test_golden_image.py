from __future__ import annotations

from pathlib import Path

import pytest
from PIL import Image

from tests.golden_image import assert_masked_golden_match


def _save(path: Path, mode: str, pixels: list[tuple[int, ...]]) -> None:
    image = Image.new(mode, (2, 2))
    image.putdata(pixels)
    image.save(path)


def test_masked_golden_ignores_transparent_reference_pixels(tmp_path: Path) -> None:
    golden = tmp_path / "golden.png"
    actual = tmp_path / "actual.png"
    _save(
        golden,
        "RGBA",
        [(1, 2, 3, 255), (4, 5, 6, 0), (7, 8, 9, 255), (10, 11, 12, 0)],
    )
    _save(actual, "RGB", [(1, 2, 3), (99, 99, 99), (7, 8, 9), (88, 88, 88)])

    result = assert_masked_golden_match(actual, golden)

    assert result.compared_pixels == 2
    assert result.masked_pixels == 2


def test_masked_golden_writes_a_mismatch_map(tmp_path: Path) -> None:
    golden = tmp_path / "golden.png"
    actual = tmp_path / "actual.png"
    diff = tmp_path / "artifacts" / "diff.png"
    _save(
        golden,
        "RGBA",
        [(1, 2, 3, 255), (4, 5, 6, 0), (7, 8, 9, 255), (10, 11, 12, 0)],
    )
    _save(actual, "RGB", [(9, 2, 3), (99, 99, 99), (7, 8, 9), (88, 88, 88)])

    with pytest.raises(AssertionError, match="1 differing visible pixels"):
        assert_masked_golden_match(actual, golden, diff_path=diff)

    with Image.open(diff) as image:
        rgba = image.convert("RGBA")
        assert rgba.getpixel((0, 0)) == (255, 0, 0, 255)
        assert rgba.getpixel((1, 0)) == (0, 0, 0, 0)


def test_masked_golden_rejects_different_dimensions(tmp_path: Path) -> None:
    golden = tmp_path / "golden.png"
    actual = tmp_path / "actual.png"
    _save(golden, "RGBA", [(1, 2, 3, 255)] * 4)
    Image.new("RGB", (1, 1), (1, 2, 3)).save(actual)

    with pytest.raises(AssertionError, match="size mismatch"):
        assert_masked_golden_match(actual, golden)
