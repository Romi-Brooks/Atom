from pathlib import Path
from colorsys import hsv_to_rgb, rgb_to_hsv
from PIL import Image, ImageDraw, ImageFilter

project = Path(__file__).resolve().parents[1]
textures = project / "Content/Textures"
source_art = project / "SourceArt"
source = Image.open(source_art / "WaterfrontOutpaint.png").convert("RGB")
roofs = Image.open(source_art / "WaterfrontRoofEdit.png").convert("RGB")
if roofs.size != source.size:
    raise ValueError("Roof edit and original panorama must share the same canvas")
# Preserve the original scenery, including the precise water-mask coordinates.
roof_mask = Image.new("L", source.size)
patches = ImageDraw.Draw(roof_mask)
patches.rectangle((491, 12, 738, 173), fill=255)
patches.rectangle((1410, 24, 1655, 163), fill=255)
source = Image.composite(roofs, source, roof_mask.filter(ImageFilter.GaussianBlur(4)))
size = (round(source.width * 315 / source.height), 315)
source.resize(size, Image.Resampling.NEAREST).save(textures / "WaterfrontPanorama.png")
street = Image.open(source_art / "BridgeStreet.png").convert("RGB")
street.resize(size, Image.Resampling.NEAREST).save(textures / "BridgeStreet.png")
mask = Image.new("L", source.size)
d = ImageDraw.Draw(mask)
for left, right in [(0, 286), (735, 1420), (1921, 2171)]:
    d.rectangle((left, 350, right, 397), fill=255)
for center in (66, 239, 781, 1069, 1378, 1962, 2132):
    d.rectangle((center-8, 349, center+8, 397), fill=0)
for center in (940, 1199):
    d.polygon([(center, 352), (center-13, 371), (center-30, 397), (center+31, 397),
               (center+13, 371)], fill=0)
mask.resize(size, Image.Resampling.NEAREST).save(textures / "WaterfrontMask.png")
puff = Image.new("RGBA", (7, 7))
d = ImageDraw.Draw(puff)
d.polygon([(0, 3), (1, 1), (3, 1), (4, 0), (5, 1), (6, 2), (6, 4),
           (5, 5), (4, 5), (3, 6), (1, 5), (0, 4)], fill=(227, 241, 255, 100))
d.polygon([(1, 3), (2, 2), (5, 2), (5, 4), (3, 5), (1, 4)], fill=(236, 246, 255, 207))
puff.save(textures / "Breath.png")
traffic = Image.open(source_art / "Traffic.png").convert("RGBA")
fleet = Image.new("RGBA", (432, 32))
fleet.paste(traffic, (0, 0))
for i in range(2):
    car = Image.new("RGBA", (108, 32))
    d = ImageDraw.Draw(car)
    body, bright = (("#4f6a7b", "#879da9"), ("#a67b3b", "#dbb66c"))[i]
    shell = ([(6, 26), (6, 7), (12, 2), (70, 2), (88, 12), (102, 15), (105, 25)]
             if i == 0 else [(4, 26), (7, 16), (25, 13), (42, 6), (71, 6), (84, 14),
                             (102, 17), (105, 25)])
    d.polygon(shell, fill=body, outline="#18192a", width=2)
    d.line(shell[1:-2], fill=bright)
    if i == 0:
        d.rectangle((12, 7, 44, 14), fill="#334658")
        d.polygon([(53, 6), (69, 6), (83, 14), (53, 14)], fill="#446275")
        d.line((48, 7, 48, 22), fill="#344457")
    else:
        d.polygon([(31, 14), (44, 8), (69, 8), (80, 14)], fill="#3e5065")
        d.rectangle((54, 3, 64, 6), fill="#ead8a5")
        for x in range(19, 85, 6):
            d.rectangle((x, 18, x+2, 19), fill="#554637")
    d.rectangle((7, 19, 10, 21), fill="#f47683")
    d.rectangle((97, 18, 103, 20), fill="#fff0bb")
    for x in (23, 84):
        d.ellipse((x-6, 20, x+6, 31), fill="#12121d")
        d.ellipse((x-3, 23, x+3, 28), fill="#555771")
    fleet.paste(car, (216 + 108*i, 0))
fleet.save(textures / "TrafficFleet.png")

pedestrian_source = Image.open(source_art / "Pedestrians.png").convert("RGBA")
pedestrian_atlas = Image.new("RGBA", (32 * 4, 46 * 12))
# Crop loose generation cells to a shared pixel scale and align boot soles.
cells = []
for row in range(4):
    for column in range(4):
        cell = pedestrian_source.crop((
            pedestrian_source.width * column // 4, pedestrian_source.height * row // 4,
            pedestrian_source.width * (column + 1) // 4, pedestrian_source.height * (row + 1) // 4,
        ))
        cell.putalpha(cell.getchannel("A").point(lambda alpha: 255 if alpha >= 155 else 0))
        cells.append(cell)
scale = 41 / max(cell.getbbox()[3] - cell.getbbox()[1] for cell in cells)
coat_palettes = (
    (0.55, (0.38, 0.04)),
    (0.10, (0.01, 0.32)),
    (0.91, (0.03, 0.59)),
    (0.40, (0.56, 0.10)),
)
for i, cell in enumerate(cells):
    row, column = divmod(i, 4)
    left, top, right, bottom = cell.getbbox()
    frame = cell.crop((left, top, right, bottom)).resize(
        (round((right - left) * scale), round((bottom - top) * scale)), Image.Resampling.NEAREST,
    )
    base = Image.new("RGBA", (32, 46))
    center = 16 + round(((left + right) / 2 - cell.width / 2) * scale)
    base.paste(frame, (center - frame.width // 2, 45 - frame.height))
    coat_hue, variants = coat_palettes[row]
    pedestrian_atlas.paste(base, (column * 32, row * 3 * 46))
    for palette, new_hue in enumerate(variants, 1):
        recolored = base.copy()
        for y in range(16, 37):
            for x in range(32):
                r, g, b, alpha = base.getpixel((x, y))
                hue, saturation, value = rgb_to_hsv(r / 255, g / 255, b / 255)
                hue_delta = (hue - coat_hue + 0.5) % 1 - 0.5
                if alpha and abs(hue_delta) < 0.11 and saturation > 0.08 and value > 0.20:
                    color = hsv_to_rgb((new_hue + hue_delta * 0.5) % 1, max(0.20, saturation * 0.8), value)
                    recolored.putpixel((x, y), tuple(round(channel * 255) for channel in color) + (alpha,))
        pedestrian_atlas.paste(recolored, (column * 32, (row * 3 + palette) * 46))
pedestrian_atlas.save(textures / "Pedestrians.png")
