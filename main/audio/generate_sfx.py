# SPDX-License-Identifier: Apache-2.0
"""Generate the ten interaction sound effects.

All sounds are synthesized here from oscillators and shaped noise, so the
project owns them outright (see SOURCES.md). Output: 24 kHz mono 16-bit WAV,
the same format as the music loop, ready to embed and mix on device.

Run: python3 generate_sfx.py
"""

import math
import random
import struct
import wave
from pathlib import Path

RATE = 24000
OUT = Path(__file__).parent


def save(name, samples):
    frames = b"".join(struct.pack("<h", max(-32767, min(32767, int(v * 32767)))) for v in samples)
    with wave.open(str(OUT / name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(frames)
    print(name, len(samples) / RATE)


def blank(seconds):
    return [0.0] * int(RATE * seconds)


def tone(buf, start, freq, seconds, gain, slide_to=None, harmonics=(1.0, 0.35, 0.12)):
    n0, n1 = int(start * RATE), int((start + seconds) * RATE)
    for i in range(n0, min(n1, len(buf))):
        t = (i - n0) / RATE
        f = freq if slide_to is None else freq + (slide_to - freq) * t / seconds
        env = math.exp(-3.2 * t / seconds)
        buf[i] += gain * env * sum(h * math.sin(2 * math.pi * f * t * (k + 1)) for k, h in enumerate(harmonics))


def noise(buf, start, seconds, gain, seed=7, decay=4.0):
    rng = random.Random(seed)
    n0, n1 = int(start * RATE), int((start + seconds) * RATE)
    last = 0.0
    for i in range(n0, min(n1, len(buf))):
        t = (i - n0) / RATE
        last = 0.72 * last + 0.28 * rng.uniform(-1, 1)
        buf[i] += gain * math.exp(-decay * t / seconds) * last / 0.35


def blue_gift():
    buf = blank(1.4)
    for i, f in enumerate((1568, 2093, 2637, 3136, 2637, 3520)):
        tone(buf, i * 0.11, f, 0.65, 0.20)
    save("sfx_blue_gift.wav", buf)


def red_gift():
    # The bang lands when the rocket bursts (ROCKET_TIME in tree_scene.c).
    bang = 1.8
    buf = blank(bang + 0.55)
    tone(buf, 0, 500, bang, 0.16, slide_to=1900, harmonics=(1.0, 0.2))
    noise(buf, bang, 0.5, 0.55, seed=11, decay=5.0)
    tone(buf, bang, 140, 0.35, 0.35, slide_to=55, harmonics=(1.0,))
    save("sfx_red_gift.wav", buf)


def bow():
    buf = blank(0.6)
    noise(buf, 0, 0.55, 0.5, seed=21, decay=2.2)
    save("sfx_bow.wav", buf)


def bell():
    buf = blank(0.9)
    for start in (0.0, 0.16):
        for f, g in ((1568, 0.22), (2093, 0.14), (2960, 0.08)):
            tone(buf, start, f, 0.55, g, harmonics=(1.0,))
    save("sfx_bell.wav", buf)


def cane():
    buf = blank(0.5)
    for i, start in enumerate((0.0, 0.07, 0.15)):
        noise(buf, start, 0.09, 0.5, seed=31 + i, decay=9.0)
    save("sfx_cane.wav", buf)


def bauble():
    buf = blank(0.7)
    noise(buf, 0, 0.25, 0.4, seed=41, decay=10.0)
    for i, f in enumerate((3400, 4300, 5100, 2900, 4700, 3800)):
        tone(buf, 0.02 + i * 0.045, f, 0.22, 0.10, harmonics=(1.0,))
    save("sfx_bauble.wav", buf)


def star():
    buf = blank(0.85)
    for i, f in enumerate((2637, 3136, 3520, 4186)):
        tone(buf, i * 0.07, f, 0.5, 0.16)
    save("sfx_star.wav", buf)


def stocking():
    buf = blank(0.8)
    for i, start in enumerate((0.0, 0.22, 0.44)):
        tone(buf, start, 700 - i * 90, 0.14, 0.32, slide_to=180, harmonics=(1.0, 0.15))
    save("sfx_stocking.wav", buf)


def star_ding():
    buf = blank(0.9)
    for start, freq in ((0.0, 2637), (0.13, 3520), (0.26, 2637)):
        tone(buf, start, freq, 0.5, 0.20, harmonics=(1.0, 0.25))
        tone(buf, start, freq * 2, 0.3, 0.06, harmonics=(1.0,))
    save("sfx_star_ding.wav", buf)


def snow_step():
    buf = blank(1.5)
    for i in range(9):
        start = i * 0.16
        noise(buf, start, 0.09, 0.42, seed=61 + i, decay=8.0)
        tone(buf, start, 110, 0.06, 0.10, slide_to=70, harmonics=(1.0,))
    save("sfx_snow_step.wav", buf)


if __name__ == "__main__":
    blue_gift()
    red_gift()
    bow()
    bell()
    cane()
    bauble()
    star()
    stocking()
    star_ding()
    snow_step()
