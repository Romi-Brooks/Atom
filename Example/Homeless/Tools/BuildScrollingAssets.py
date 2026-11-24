from pathlib import Path
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
