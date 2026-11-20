#!/usr/bin/env python3
"""Generates icons/menu.png: a 64x64 hamburger icon (three white rounded bars
with a dark outline on a translucent dark rounded square), antialiased by 4x
supersampling. The build scales it to CYD_MENU_ICON_SIZE.

Usage: make_menu_icon.py [output.png]
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from png2rgb565 import encode_png  # noqa: E402

SIZE = 64
SS = 4  # supersampling factor


def rounded_rect(px, py, x0, y0, x1, y1, r):
    """True if (px, py) lies inside the rounded rectangle [x0, x1) x [y0, y1)."""
    if px < x0 or px >= x1 or py < y0 or py >= y1:
        return False
    cx = min(max(px, x0 + r), x1 - r)
    cy = min(max(py, y0 + r), y1 - r)
    return (px - cx) ** 2 + (py - cy) ** 2 <= r * r


def sample(px, py):
    """Colour (r, g, b, a) at a point in 64x64 icon space."""
    bars = (16, 32, 48)  # bar centre lines
    for cy in bars:
        if rounded_rect(px, py, 12, cy - 4, 52, cy + 4, 4):
            return (255, 255, 255, 255)
    for cy in bars:
        if rounded_rect(px, py, 10, cy - 6, 54, cy + 6, 6):
            return (16, 16, 24, 230)
    if rounded_rect(px, py, 0, 0, SIZE, SIZE, 12):
        return (16, 16, 24, 120)
    return (0, 0, 0, 0)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "icons", "menu.png")
    pixels = []
    for y in range(SIZE):
        for x in range(SIZE):
            acc = [0, 0, 0, 0]
            for sy in range(SS):
                for sx in range(SS):
                    r, g, b, a = sample(x + (sx + 0.5) / SS, y + (sy + 0.5) / SS)
                    acc[0] += r * a
                    acc[1] += g * a
                    acc[2] += b * a
                    acc[3] += a
            n = SS * SS
            a = acc[3] / n
            if a == 0:
                pixels.append((0, 0, 0, 0))
            else:
                pixels.append((round(acc[0] / acc[3]), round(acc[1] / acc[3]),
                               round(acc[2] / acc[3]), round(a)))
    with open(out, "wb") as f:
        f.write(encode_png(SIZE, SIZE, pixels))


if __name__ == "__main__":
    main()
