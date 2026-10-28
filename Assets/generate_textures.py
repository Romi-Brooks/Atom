"""Generates the placeholder art for Example/Game (T1.3 smoke scene).

These are deliberately tiny, deterministic PNGs: the point is to have a real
res:// content root with real decodable files, not to ship art. The script lives
next to the assets it writes, so the art stays reproducible:

    python Assets/generate_textures.py
"""

from pathlib import Path

from PIL import Image, ImageDraw

# The script sits in Assets/, so its own directory is the content root.
OUT = Path(__file__).resolve().parent / "Textures"


def save(image: Image.Image, name: str) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    image.save(OUT / name, format="PNG", optimize=True)
    print(f"wrote {OUT / name}")


def player() -> Image.Image:
    """16x16 knight-ish blob with a 1px outline and a bright facing mark."""
    image = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rectangle((2, 2, 13, 13), fill=(92, 168, 255, 255), outline=(20, 40, 80, 255))
    draw.rectangle((5, 5, 7, 7), fill=(255, 255, 255, 255))
    draw.rectangle((9, 5, 11, 7), fill=(255, 255, 255, 255))
    # A single bright pixel column on the right edge marks the facing direction,
    # so flip_x is visible at a glance.
    draw.rectangle((13, 6, 13, 9), fill=(255, 220, 90, 255))
    return image


def crate() -> Image.Image:
    image = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 15, 15), fill=(170, 118, 72, 255), outline=(96, 62, 34, 255))
    draw.line((0, 0, 15, 15), fill=(120, 80, 46, 255))
    draw.line((15, 0, 0, 15), fill=(120, 80, 46, 255))
    return image


def floor() -> Image.Image:
    image = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 15, 15), fill=(58, 74, 60, 255))
    draw.point((4, 4), fill=(74, 94, 76, 255))
    draw.point((11, 9), fill=(74, 94, 76, 255))
    return image


def marker() -> Image.Image:
    """The trigger/exit marker: a flag on a transparent 16x16 tile."""
    image = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rectangle((7, 2, 8, 15), fill=(210, 210, 210, 255))
    draw.polygon(((9, 3), (15, 5), (9, 8)), fill=(255, 90, 120, 255))
    return image


def main() -> None:
    save(player(), "player.png")
    save(crate(), "crate.png")
    save(floor(), "floor.png")
    save(marker(), "marker.png")
    # A 4x4 atlas: four 8x8 cells, used to prove the source-rect path works.
    atlas = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    draw = ImageDraw.Draw(atlas)
    for index, tint in enumerate([(220, 70, 70), (70, 200, 120), (90, 140, 240), (230, 200, 80)]):
        x = (index % 2) * 8
        y = (index // 2) * 8
        draw.rectangle((x + 1, y + 1, x + 6, y + 6), fill=(*tint, 255))
    save(atlas, "atlas.png")


if __name__ == "__main__":
    main()
