"""Build the original sprite/audio placeholders and import the supplied promenade art."""
import argparse
import math
import random
import wave
from array import array
from pathlib import Path

from PIL import Image, ImageDraw

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Content"
SOURCE_ART = PROJECT / "SourceArt"


def make_background(reference):
    art = Image.open(reference).convert("RGB")
    art.resize((560, 315), Image.Resampling.NEAREST).save(SOURCE_ART / "Promenade.png")
    # Coordinates trace the water and foreground silhouettes in the supplied plate.
    mask = Image.new("L", (1672, 941))
    draw = ImageDraw.Draw(mask)
    draw.rectangle((280, 416, 1423, 487), fill=255)
    for box in [(304, 414, 411, 428), (348, 414, 368, 500),
                (778, 414, 888, 429), (823, 414, 844, 500),
                (1292, 414, 1403, 429), (1336, 414, 1359, 500)]:
        draw.rectangle(box, fill=0)
    for x in (623, 1058):
        draw.polygon([(x, 422), (x-16, 449), (x-27, 467), (x-40, 491),
                      (x+41, 491), (x+29, 467), (x+16, 449)], fill=0)
    mask.resize((560, 315), Image.Resampling.NEAREST).save(SOURCE_ART / "WaterMask.png")


def make_walker():
    sheet = Image.new("RGBA", (32 * 8, 46))
    poses = [(0, 0), (0, 0), (-5, 5), (-3, 3), (0, 1), (5, -5), (3, -3), (1, 0)]
    outline, light, red, dark = "#18172b", "#b77163", "#783b4c", "#4b2a42"
    for i, (back, front) in enumerate(poses):
        frame = Image.new("RGBA", (32, 46))
        d = ImageDraw.Draw(frame)
        bob = [0, 1, 0, 1, 2, 0, 1, 2][i]
        # Behind the coat, the backpack shifts with the torso.
        d.polygon([(9, 16+bob), (6, 18+bob), (4, 25+bob), (5, 32+bob),
                   (11, 33+bob), (13, 20+bob)], fill=outline)
        d.rectangle((6, 22+bob, 10, 29+bob), fill="#465369")
        d.line([(6, 22+bob), (8, 20+bob), (10, 20+bob)], fill="#8293a3")
        for dx, jeans, trim in [(back, "#33324d", "#58617d"), (front, "#3c4b68", "#7285a0")]:
            lift = int(abs(dx) < 4 and dx < 0) * 2
            d.line([(15, 31+bob), (15+dx//2, 36), (15+dx, 42-lift)], fill=outline, width=6)
            d.line([(15, 31+bob), (15+dx//2, 36), (15+dx, 42-lift)], fill=jeans, width=4)
            d.point((15+dx//2, 36), fill=trim)
            d.rectangle((13+dx, 41-lift, 19+dx, 44-lift), fill=outline)
            d.line((15+dx, 42-lift, 18+dx, 42-lift), fill="#77718a")
        d.polygon([(12, 16+bob), (20, 16+bob), (23, 22+bob), (22, 32+bob),
                   (20, 35+bob), (10, 34+bob), (9, 25+bob)], fill=outline)
        d.polygon([(13, 17+bob), (19, 17+bob), (21, 22+bob), (20, 33+bob),
                   (12, 33+bob), (11, 25+bob)], fill=red)
        d.line((20, 22+bob, 19, 31+bob), fill=light)
        d.line((12, 30+bob, 19, 31+bob), fill=dark)
        d.line([(15, 22+bob), (15-front//3, 27+bob), (16-front//2, 31+bob)], fill=dark, width=4)
        d.point((16-front//2, 32+bob), fill=light)
        d.polygon([(9, 7+bob), (13, 4+bob), (19, 5+bob), (23, 8+bob), (24, 14+bob),
                   (21, 17+bob), (15, 17+bob), (10, 14+bob)], fill=outline)
        d.polygon([(15, 8+bob), (20, 8+bob), (23, 12+bob), (24, 13+bob), (22, 15+bob),
                   (21, 17+bob), (17, 16+bob), (14, 13+bob)], fill="#c48b73")
        d.point((21, 12+bob), fill=outline)
        d.polygon([(10, 7+bob), (13, 5+bob), (19, 5+bob), (22, 8+bob), (20, 10+bob),
                   (16, 10+bob), (15, 14+bob), (11, 14+bob)], fill="#333751")
        d.line((12, 8+bob, 20, 8+bob), fill="#596787")
        d.polygon([(12, 16+bob), (22, 16+bob), (22, 19+bob), (16, 20+bob),
                   (11-front//3, 23+bob), (10-front//3, 21+bob)], fill="#bd9b65")
        d.line((16, 17+bob, 22, 17+bob), fill="#ebc383")
        sheet.paste(frame, (i*32, 0))
    sheet.save(ROOT / "Textures/Walker.png")


def make_cars():
    sheet = Image.new("RGBA", (108 * 2, 32))
    for i, (body, lit) in enumerate([("#463b67", "#7d6e9e"), ("#5c3949", "#a0636c")]):
        car = Image.new("RGBA", (108, 32))
        d = ImageDraw.Draw(car)
        d.polygon([(3, 23), (6, 15), (25, 13), (35, 4), (67, 4), (84, 12),
                   (99, 14), (104, 18), (105, 26), (4, 27)], fill="#151626")
        d.polygon([(6, 22), (8, 16), (26, 15), (36, 6), (66, 6), (83, 14),
                   (99, 16), (103, 20), (103, 24), (6, 25)], fill=body)
        d.line([(9, 16), (26, 15), (37, 6), (66, 6), (83, 14), (98, 16)], fill=lit)
        d.polygon([(30, 14), (38, 8), (50, 8), (50, 14)], fill="#303c58")
        d.polygon([(53, 8), (65, 8), (78, 14), (53, 14)], fill="#3e5269")
        d.line((58, 9, 66, 9), fill="#7894a3")
        d.line((52, 16, 52, 24), fill="#28253b")
        d.line((69, 17, 73, 17), fill=lit)
        d.rectangle((6, 18, 10, 20), fill="#d66572")
        d.rectangle((97, 18, 103, 20), fill="#fff0b4")
        d.line((7, 24, 102, 24), fill="#4e475d")
        for x in (20, 85):
            d.ellipse((x-7, 19, x+7, 31), fill="#10121e")
            d.ellipse((x-4, 22, x+4, 29), fill="#41425a")
            d.rectangle((x-1, 24, x+1, 27), fill="#85818e")
        sheet.paste(car, (i*108, 0))
    sheet.save(SOURCE_ART / "Traffic.png")


def make_audio():
    sample_rate = 24000
    seconds = 16
    rng = random.Random(2005)
    samples = array("h")
    wind = 0
    for i in range(seconds * sample_rate):
        t = i / sample_rate
        phase = 2 * math.pi * t / seconds
        wind = 0.984 * wind + 0.016 * rng.uniform(-1, 1)
        pad = sum(math.sin(phase * cycles) for cycles in (1047, 1568, 2093, 2489))
        hum = math.sin(phase * 480) + 0.4 * math.sin(phase * 720)
        edge = min(1, t * 8, (seconds-t) * 8)
        value = edge * (0.44 * wind * (0.6 + 0.24 * math.sin(phase * 3)) + 0.021 * hum
                        + 0.012 * pad * (0.6 + 0.35 * math.cos(phase)))
        samples.append(round(32767 * max(-1, min(1, 2.3 * value))))
    with wave.open(str(ROOT / "Audio/Waterfront.wav"), "wb") as wav:
        wav.setparams((1, 2, sample_rate, 0, "NONE", "not compressed"))
        wav.writeframes(samples.tobytes())


def make_footsteps():
    rate = 24000
    for index, (duration, toe_delay, pitch) in enumerate(
        [(0.145, 0.032, 89), (0.164, 0.045, 80), (0.155, 0.038, 98)], start=1
    ):
        rng = random.Random(2005 + index)
        samples = []
        low = 0.0
        previous = 0.0
        for i in range(round(rate * duration)):
            t = i / rate
            noise = rng.uniform(-1.0, 1.0)
            low = 0.83 * low + 0.17 * noise
            crunch = 0.7 * low + 0.16 * (noise - previous)
            previous = noise
            heel = min(1.0, t * 520) * math.exp(-36 * t)
            toe_time = max(0.0, t - toe_delay)
            toe = min(1.0, toe_time * 310) * math.exp(-44 * toe_time)
            thump = math.sin(2 * math.pi * (pitch - 135 * t) * t) * heel
            end_fade = min(1.0, (duration - t) * 100)
            samples.append(end_fade * (crunch * (heel + 0.55 * toe) + 0.14 * thump))
        gain = 0.55 / max(map(abs, samples))
        pcm = array("h", (round(x * gain * 32767) for x in samples))
        with wave.open(str(ROOT / f"Audio/Footstep{index}.wav"), "wb") as wav:
            wav.setparams((1, 2, rate, 0, "NONE", "not compressed"))
            wav.writeframes(pcm.tobytes())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path, help="the supplied image #4")
    make_background(parser.parse_args().reference)
    make_walker()
    make_cars()
    Image.new("RGBA", (1, 1), "white").save(ROOT / "Textures/Snow.png")
    make_audio()
    make_footsteps()
