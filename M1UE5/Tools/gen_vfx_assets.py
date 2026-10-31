"""Gen stage: procedural VFX / decal source PNGs (ORIGINAL art, no M1 extraction).

  T_muzzle_flash.png -- an additive warm radial flash with spikes, composited over
                        the weapon viewmodel during the fire window (AM1HUD).
  T_scorch_decal.png -- a dark charred splat with an irregular radial alpha mask,
                        projected at projectile impacts (AM1Impact / ue5_import_decal).

Both are clean-room procedural art (not derived from Bungie pixels), so they can
ship. Consumed by Tools/ue5_import_vfx.py and Tools/ue5_import_decal.py.

Run:  python Tools/gen_vfx_assets.py   ->  Reference/Sprites/T_muzzle_flash.png + T_scorch_decal.png
"""
import math
import os
import random

from PIL import Image

_TOOLS = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(_TOOLS), "Reference", "Sprites")
SIZE = 256


def muzzle_flash():
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    px = img.load()
    c = SIZE / 2.0
    for y in range(SIZE):
        for x in range(SIZE):
            dx, dy = x - c, y - c
            r = math.hypot(dx, dy) / c
            ang = math.atan2(dy, dx)
            core = max(0.0, 1.0 - r * 1.6)               # soft central glow
            spike = max(0.0, 1.0 - r * 1.05) * (0.5 + 0.5 * abs(math.cos(ang * 3)))
            v = min(1.0, core * 1.2 + spike * 0.9)
            if v <= 0.01:
                continue
            px[x, y] = (int(255 * min(1.0, v * 1.1)), int(235 * v), int(150 * v * v),
                        int(255 * v))                    # warm white -> yellow, alpha = intensity
    return img


def scorch_decal():
    img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    px = img.load()
    c = SIZE / 2.0
    random.seed(7)
    jit = [0.80 + 0.35 * random.random() for _ in range(360)]  # irregular rim per angle
    for y in range(SIZE):
        for x in range(SIZE):
            dx, dy = x - c, y - c
            r = math.hypot(dx, dy) / c
            ang = int(math.degrees(math.atan2(dy, dx))) % 360
            v = max(0.0, 1.0 - r / jit[ang])
            if v <= 0.02:
                continue
            v = v ** 1.4
            s = int(22 + 24 * (1.0 - v))                 # charcoal, a touch lighter at the rim
            px[x, y] = (s, int(s * 0.85), int(s * 0.72), int(235 * v))
    return img


def main():
    os.makedirs(OUT, exist_ok=True)
    muzzle_flash().save(os.path.join(OUT, "T_muzzle_flash.png"))
    scorch_decal().save(os.path.join(OUT, "T_scorch_decal.png"))
    print("[vfx] wrote T_muzzle_flash.png + T_scorch_decal.png ->", OUT)


if __name__ == "__main__":
    main()
