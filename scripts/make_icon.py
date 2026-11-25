"""Draw the front page's icon (runtime/U9ChtFront/u9cht.ico): a gold ankh on a dark tile.

Drawn from plain geometry, with no game art. Standard library only. The .ico is committed; run this again only to
change the drawing. `--preview FILE.png` also writes the sizes side by side.
"""
import argparse
import struct
import zlib
from pathlib import Path

SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)
SAMPLES = 8
TILE, EDGE = (38, 29, 21), (150, 118, 62)
TOP, BOTTOM = (250, 218, 140), (196, 142, 58)


def in_tile(x, y, inset, radius):
    """Rounded square in a 100x100 drawing."""
    dx = max(inset + radius - x, x - (100 - inset - radius), 0)
    dy = max(inset + radius - y, y - (100 - inset - radius), 0)
    return dx * dx + dy * dy <= radius * radius


def in_ankh(x, y, bold):
    # Loop: an egg that narrows towards the crossing.
    def egg(rx, ry, cy):
        t = (y - cy) / ry
        if abs(t) >= 1:
            return False
        width = rx * (1 - t * t) ** 0.5 * (1 - 0.28 * t)
        return abs(x - 50) <= width

    if egg(18 + bold, 22 + bold, 30) and not egg(10 - bold / 2, 13.5 - bold / 2, 29):
        return True
    if 48 <= y <= 90 and abs(x - 50) <= 4.5 + bold + (y - 48) / 42 * 4:
        return True
    return abs(x - 50) <= 31 and abs(y - 55) <= 4.5 + bold + abs(x - 50) / 31 * 3.5


def render(size):
    bold = 2.5 if size <= 24 else 1.2 if size <= 48 else 0
    edge = max(2.5, 100 / size) if size >= 32 else 0
    out = bytearray()
    for py in range(size):
        for px in range(size):
            total, covered = [0.0, 0.0, 0.0], 0
            for s in range(SAMPLES * SAMPLES):
                x = (px + (s % SAMPLES + 0.5) / SAMPLES) * 100 / size
                y = (py + (s // SAMPLES + 0.5) / SAMPLES) * 100 / size
                if not in_tile(x, y, 0, 16):
                    continue
                covered += 1
                if in_ankh(x, y, bold):
                    t = min(max((y - 8) / 82, 0), 1)
                    color = [a + (b - a) * t for a, b in zip(TOP, BOTTOM)]
                elif edge and not in_tile(x, y, edge, 16 - edge):
                    color = EDGE
                else:
                    color = TILE
                for i in range(3):
                    total[i] += color[i]
            out += bytes(round(v / covered) if covered else 0 for v in total)
            out.append(round(255 * covered / SAMPLES ** 2))
    return bytes(out)


def png(width, height, rgba):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    rows = b"".join(b"\0" + rgba[y * width * 4:(y + 1) * width * 4] for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def dib(size, rgba):
    """32-bit icon bitmap: header, bottom-up BGRA, then an all-zero AND mask."""
    pixels = bytearray()
    for y in reversed(range(size)):
        for x in range(size):
            r, g, b, a = rgba[(y * size + x) * 4:(y * size + x) * 4 + 4]
            pixels += bytes((b, g, r, a))
    mask = bytes((size + 31) // 32 * 4 * size)
    return struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0, len(pixels) + len(mask), 0, 0, 0, 0) + pixels + mask


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parents[1] / "runtime/U9ChtFront/u9cht.ico")
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    images = [(s, render(s)) for s in SIZES]
    blobs = [png(s, s, p) if s >= 128 else dib(s, p) for s, p in images]
    offset, head = 6 + 16 * len(blobs), struct.pack("<HHH", 0, 1, len(blobs))
    for (s, _), blob in zip(images, blobs):
        head += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)
    args.output.write_bytes(head + b"".join(blobs))
    if args.preview:
        width, height = sum(s + 8 for s in SIZES), max(SIZES)
        sheet = bytearray(b"\xf0\xf0\xf0\xff" * (width * height))
        left = 0
        for s, p in images:
            for i in range(s * s):
                r, g, b, a = p[i * 4:i * 4 + 4]
                at = (i // s * width + left + i % s) * 4
                sheet[at:at + 3] = bytes((c * a + 240 * (255 - a)) // 255 for c in (r, g, b))
            left += s + 8
        args.preview.write_bytes(png(width, height, bytes(sheet)))


if __name__ == "__main__":
    main()
