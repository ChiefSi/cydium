"""Tests for src/components/ui/tools/png2rgb565.py (run by `./build.sh --test`)."""

import os
import struct
import sys
import tempfile
import unittest
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.join(HERE, "..", "components", "ui", "tools")
sys.path.insert(0, TOOLS)

import png2rgb565 as p  # noqa: E402


def chunk(ctype, body):
    return (struct.pack(">I", len(body)) + ctype + body +
            struct.pack(">I", zlib.crc32(ctype + body) & 0xFFFFFFFF))


def palette_png(width, height, depth, rows, palette, trns=b""):
    """Builds a palette PNG; rows are lists of palette indices."""
    raw = bytearray()
    per_byte = 8 // depth
    for row in rows:
        raw.append(0)
        packed = bytearray((width + per_byte - 1) // per_byte)
        for x, idx in enumerate(row):
            packed[x // per_byte] |= idx << (8 - depth * (x % per_byte + 1))
        raw += packed
    data = p.PNG_SIG + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, depth, 3, 0, 0, 0))
    data += chunk(b"PLTE", b"".join(bytes(c) for c in palette))
    if trns:
        data += chunk(b"tRNS", trns)
    return data + chunk(b"IDAT", zlib.compress(bytes(raw))) + chunk(b"IEND", b"")


class Png2Rgb565Test(unittest.TestCase):
    def test_rgba_round_trip(self):
        px = [(255, 0, 0, 255), (0, 255, 0, 128), (0, 0, 255, 0), (10, 20, 30, 40)]
        w, h, out = p.decode_png(p.encode_png(2, 2, px))
        self.assertEqual((w, h), (2, 2))
        self.assertEqual(out, px)

    def test_palette_with_transparency(self):
        data = palette_png(3, 1, 2, [[0, 1, 2]], [(255, 0, 0), (0, 255, 0), (0, 0, 255)],
                           trns=b"\xff\x00")
        _, _, out = p.decode_png(data)
        self.assertEqual(out, [(255, 0, 0, 255), (0, 255, 0, 0), (0, 0, 255, 255)])

    def test_fit_square_centres_and_scales(self):
        # 2x1 opaque white scaled into 4x4: rows 1-2 filled, rows 0 and 3 clear.
        out = p.fit_square(2, 1, [(255, 255, 255, 255)] * 2, 4)
        alpha = [a for _, _, _, a in out]
        self.assertEqual(alpha, [0] * 4 + [255] * 8 + [0] * 4)

    def test_downscale_averages_premultiplied(self):
        # Opaque red next to fully transparent green: no green bleeds in.
        out = p.fit_square(2, 2, [(255, 0, 0, 255), (0, 255, 0, 0)] * 2, 1)
        self.assertEqual(out, [(255, 0, 0, 128)])

    def test_rgb565_be(self):
        self.assertEqual(p.rgb565_be(255, 0, 0), 0x00F8)
        self.assertEqual(p.rgb565_be(0, 0, 255), 0x1F00)

    def test_rejects_interlaced(self):
        data = p.encode_png(1, 1, [(0, 0, 0, 255)])
        ihdr = struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 1)
        data = data[:8] + chunk(b"IHDR", ihdr) + data[8 + 25:]
        with self.assertRaises(p.PngError):
            p.decode_png(data)

    def test_default_icon_converts(self):
        icon = os.path.join(HERE, "..", "components", "ui", "icons", "menu.png")
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "icon.h")
            p.main(["--size", "24", "--name", "kTestIcon", icon, out])
            with open(out) as f:
                header = f.read()
        self.assertIn("constexpr int kTestIconSize = 24;", header)
        self.assertIn("kTestIconPixels[576]", header)
        self.assertIn("kTestIconAlpha[576]", header)


if __name__ == "__main__":
    unittest.main()
