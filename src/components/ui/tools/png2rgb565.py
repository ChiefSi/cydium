#!/usr/bin/env python3
"""Convert a PNG to a C++ header of big-endian RGB565 pixels plus 8-bit alpha.

Standard library only (zlib, struct), so it runs with ESP-IDF's Python and on
any host without Pillow. Supports non-interlaced PNGs with bit depth 8 in
greyscale, RGB, grey+alpha and RGBA, and palette images at 1, 2, 4 or 8 bits
(with tRNS transparency). The image is scaled to fit a size x size square
(area averaging with premultiplied alpha) and centred on transparency.

Usage: png2rgb565.py --size N --name kMenuIcon input.png output.h
"""

import argparse
import struct
import sys
import zlib

PNG_SIG = b"\x89PNG\r\n\x1a\n"


class PngError(ValueError):
    pass


def _chunks(data):
    if data[:8] != PNG_SIG:
        raise PngError("not a PNG file")
    pos = 8
    while pos + 8 <= len(data):
        length, ctype = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if len(body) != length:
            raise PngError("truncated chunk")
        yield ctype, body
        pos += 12 + length
        if ctype == b"IEND":
            return
    raise PngError("missing IEND")


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def _unfilter(raw, width, height, bpp, row_bytes):
    out = bytearray()
    prev = bytearray(row_bytes)
    pos = 0
    for _ in range(height):
        ftype = raw[pos]
        line = bytearray(raw[pos + 1:pos + 1 + row_bytes])
        pos += 1 + row_bytes
        if len(line) != row_bytes:
            raise PngError("truncated image data")
        for i in range(row_bytes):
            left = line[i - bpp] if i >= bpp else 0
            up = prev[i]
            ul = prev[i - bpp] if i >= bpp else 0
            if ftype == 1:
                line[i] = (line[i] + left) & 0xFF
            elif ftype == 2:
                line[i] = (line[i] + up) & 0xFF
            elif ftype == 3:
                line[i] = (line[i] + ((left + up) >> 1)) & 0xFF
            elif ftype == 4:
                line[i] = (line[i] + _paeth(left, up, ul)) & 0xFF
            elif ftype != 0:
                raise PngError("bad filter type %d" % ftype)
        out += line
        prev = line
    return out


def decode_png(data):
    """Returns (width, height, pixels) with pixels a list of (r, g, b, a)."""
    ihdr = None
    palette = []
    trns = b""
    idat = bytearray()
    for ctype, body in _chunks(data):
        if ctype == b"IHDR":
            ihdr = struct.unpack(">IIBBBBB", body)
        elif ctype == b"PLTE":
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif ctype == b"tRNS":
            trns = body
        elif ctype == b"IDAT":
            idat += body
    if ihdr is None:
        raise PngError("missing IHDR")
    width, height, depth, ctype, _, _, interlace = ihdr
    if interlace:
        raise PngError("interlaced PNGs are not supported")
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(ctype)
    if channels is None:
        raise PngError("unknown colour type %d" % ctype)
    if ctype == 3:
        if depth not in (1, 2, 4, 8):
            raise PngError("unsupported palette bit depth %d" % depth)
    elif depth != 8:
        raise PngError("only 8-bit channels are supported (got %d)" % depth)

    bits = channels * depth
    row_bytes = (width * bits + 7) // 8
    bpp = max(1, bits // 8)
    raw = _unfilter(zlib.decompress(bytes(idat)), width, height, bpp, row_bytes)

    pixels = []
    for y in range(height):
        row = raw[y * row_bytes:(y + 1) * row_bytes]
        for x in range(width):
            if ctype == 3:
                per_byte = 8 // depth
                byte = row[x // per_byte]
                shift = 8 - depth * (x % per_byte + 1)
                idx = (byte >> shift) & ((1 << depth) - 1)
                if idx >= len(palette):
                    raise PngError("palette index out of range")
                r, g, b = palette[idx]
                a = trns[idx] if idx < len(trns) else 255
            elif ctype == 0:
                v = row[x]
                r = g = b = v
                a = 0 if len(trns) >= 2 and v == trns[1] else 255
            elif ctype == 4:
                v, a = row[2 * x], row[2 * x + 1]
                r = g = b = v
            elif ctype == 2:
                r, g, b = row[3 * x:3 * x + 3]
                a = 255
                if len(trns) >= 6 and (r, g, b) == (trns[1], trns[3], trns[5]):
                    a = 0
            else:
                r, g, b, a = row[4 * x:4 * x + 4]
            pixels.append((r, g, b, a))
    return width, height, pixels


def encode_png(width, height, pixels):
    """Encodes (r, g, b, a) pixels as an 8-bit RGBA PNG."""
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for r, g, b, a in pixels[y * width:(y + 1) * width]:
            raw += bytes((r, g, b, a))

    def chunk(ctype, body):
        crc = zlib.crc32(ctype + body) & 0xFFFFFFFF
        return struct.pack(">I", len(body)) + ctype + body + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (PNG_SIG + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) +
            chunk(b"IEND", b""))


def _weights(src, dst, offset, span):
    """Area-average weights mapping src samples onto dst samples [offset, offset+span)."""
    out = []
    scale = src / span
    for d in range(dst):
        rel = d - offset
        if rel < 0 or rel >= span:
            out.append([])
            continue
        lo, hi = rel * scale, (rel + 1) * scale
        w = []
        s = int(lo)
        while s < hi and s < src:
            overlap = min(hi, s + 1) - max(lo, s)
            if overlap > 0:
                w.append((s, overlap / scale))
            s += 1
        out.append(w)
    return out


def fit_square(width, height, pixels, size):
    """Scales to fit size x size, preserving aspect, centred on transparency."""
    if width >= height:
        sw, sh = size, max(1, round(height * size / width))
    else:
        sw, sh = max(1, round(width * size / height)), size
    ox, oy = (size - sw) // 2, (size - sh) // 2
    wx = _weights(width, size, ox, sw)
    wy = _weights(height, size, oy, sh)
    # Premultiply so transparent pixels don't bleed colour into edges.
    pm = [(r * a, g * a, b * a, a) for r, g, b, a in pixels]
    out = []
    for dy in range(size):
        for dx in range(size):
            acc = [0.0, 0.0, 0.0, 0.0]
            for sy, fy in wy[dy]:
                base = sy * width
                for sx, fx in wx[dx]:
                    f = fx * fy
                    p = pm[base + sx]
                    for i in range(4):
                        acc[i] += p[i] * f
            a = acc[3]
            if a <= 0:
                out.append((0, 0, 0, 0))
            else:
                out.append(tuple(min(255, round(c / a)) for c in acc[:3]) +
                           (min(255, round(a)),))
    return out


def rgb565_be(r, g, b):
    c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return ((c >> 8) | (c << 8)) & 0xFFFF


def to_header(name, size, pixels, source):
    def rows(values, fmt, per_line):
        lines = []
        for i in range(0, len(values), per_line):
            lines.append("    " + ", ".join(fmt % v for v in values[i:i + per_line]) + ",")
        return "\n".join(lines)

    colours = [rgb565_be(r, g, b) for r, g, b, _ in pixels]
    alpha = [a for _, _, _, a in pixels]
    return (
        "#pragma once\n"
        "// Generated by png2rgb565.py from %s. Do not edit.\n\n"
        "#include <cstdint>\n\n"
        "namespace ui\n{\n\n"
        "constexpr int %sSize = %d;\n\n"
        "// Big-endian RGB565, row-major.\n"
        "inline constexpr uint16_t %sPixels[%d] = {\n%s\n};\n\n"
        "// 0 = transparent, 255 = opaque.\n"
        "inline constexpr uint8_t %sAlpha[%d] = {\n%s\n};\n\n"
        "}  // namespace ui\n" %
        (source, name, size, name, size * size, rows(colours, "0x%04X", 12), name, size * size,
         rows(alpha, "%3d", 16)))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--size", type=int, required=True, help="output width and height in pixels")
    ap.add_argument("--name", default="kIcon", help="C++ identifier prefix")
    ap.add_argument("input")
    ap.add_argument("output")
    args = ap.parse_args(argv)
    if not 1 <= args.size <= 256:
        ap.error("--size must be 1..256")
    try:
        with open(args.input, "rb") as f:
            w, h, px = decode_png(f.read())
    except (OSError, PngError, zlib.error) as e:
        sys.exit("png2rgb565: %s: %s" % (args.input, e))
    header = to_header(args.name, args.size, fit_square(w, h, px, args.size),
                       args.input.replace("\\", "/").split("/")[-1])
    with open(args.output, "w") as f:
        f.write(header)


if __name__ == "__main__":
    main()
