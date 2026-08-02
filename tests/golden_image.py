"""Exact masked golden-image assertions for GUI tests.

The golden image's alpha channel is a binary comparison mask: transparent
pixels are ignored and every pixel with alpha greater than zero must have the
same RGB components in the captured screenshot.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Union

from PIL import Image


PathLike = Union[str, Path]


@dataclass(frozen=True)
class MaskedGoldenComparison:
    """Summary returned when a masked golden comparison succeeds."""

    compared_pixels: int
    masked_pixels: int


def assert_masked_golden_match(
    actual_path: PathLike,
    golden_path: PathLike,
    *,
    diff_path: PathLike | None = None,
) -> MaskedGoldenComparison:
    """Assert that visible golden pixels exactly match ``actual_path``.

    The golden alpha channel is a mask only: alpha == 0 ignores that location;
    alpha > 0 compares its RGB components exactly. The screenshot alpha is
    intentionally not compared because compositor screenshot formats may omit
    it or always write it as opaque. A failure writes an RGBA PNG whose opaque
    red pixels identify mismatches; all matching and masked pixels are
    transparent.
    """

    actual = Path(actual_path)
    golden = Path(golden_path)
    output = (
        Path(diff_path)
        if diff_path is not None
        else actual.with_name(f"{actual.stem}-golden-diff.png")
    )

    actual_image = _load_rgba(actual)
    golden_image = _load_rgba(golden)
    if actual_image.size != golden_image.size:
        raise AssertionError(
            "masked golden size mismatch: "
            f"actual {actual} is {actual_image.size}, "
            f"golden {golden} is {golden_image.size}"
        )

    compared = 0
    masked = 0
    mismatch_count = 0
    examples: list[tuple[int, int, tuple[int, int, int], tuple[int, int, int]]] = []
    diff = Image.new("RGBA", golden_image.size, (0, 0, 0, 0))
    actual_pixels = actual_image.load()
    golden_pixels = golden_image.load()
    diff_pixels = diff.load()
    width, height = golden_image.size

    for y in range(height):
        for x in range(width):
            actual_pixel = actual_pixels[x, y]
            golden_pixel = golden_pixels[x, y]
            expected_rgb = golden_pixel[:3]
            if golden_pixel[3] == 0:
                masked += 1
                continue

            compared += 1
            actual_rgb = actual_pixel[:3]
            if actual_rgb == expected_rgb:
                continue

            mismatch_count += 1
            diff_pixels[x, y] = (255, 0, 0, 255)
            if len(examples) < 8:
                examples.append((x, y, expected_rgb, actual_rgb))

    if compared == 0:
        raise ValueError(f"masked golden has no visible pixels: {golden}")
    if mismatch_count == 0:
        return MaskedGoldenComparison(compared, masked)

    output.parent.mkdir(parents=True, exist_ok=True)
    diff.save(output, format="PNG")
    example_text = "; ".join(
        f"({x}, {y}): expected {expected}, got {observed}"
        for x, y, expected, observed in examples
    )
    raise AssertionError(
        f"masked golden mismatch: {mismatch_count} differing visible pixels "
        f"out of {compared}; golden={golden}; actual={actual}; diff={output}; "
        f"first differences: {example_text}"
    )


def _load_rgba(path: Path) -> Image.Image:
    with Image.open(path) as source:
        image = source.convert("RGBA")
    image.load()
    return image
