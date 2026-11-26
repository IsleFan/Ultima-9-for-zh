"""bitmap16.flx textures: read one entry, decode and rebuild its 16-bit frame, patch it in place.

Only the layout the signpost textures use is supported (docs/formats/U9-BITMAP16-NOTES.md):
one frame, RGB565 (frame flags 0x60000400), followed by its mipmaps. Anything else is refused,
so unknown bytes are never regenerated. The file is read by offset; it is never loaded whole
(bitmap16.flx is about 320 MB).
"""

import hashlib
import struct
from dataclasses import dataclass, field

TABLE = 0x80
FLAGS_RGB565 = 0x60000400
FRAME_HEADER = 0x14


class FormatError(ValueError):
    pass


def digest(data):
    return hashlib.sha256(data).hexdigest()


def entry_location(f, index):
    """(offset, length) of slot `index` in an open FLX file."""
    f.seek(0x50)
    count = struct.unpack("<I", f.read(4))[0]
    if not 0 <= index < count:
        raise FormatError(f"entry {index} outside the {count} slots")
    f.seek(TABLE + 8 * index)
    return struct.unpack("<II", f.read(8))


def read_entry(path, index):
    with open(path, "rb") as f:
        offset, length = entry_location(f, index)
        if not length:
            raise FormatError(f"entry {index} is empty")
        f.seek(offset)
        data = f.read(length)
    if len(data) != length:
        raise FormatError(f"entry {index} is cut short")
    return data


@dataclass
class Texture:
    """One RGB565 frame and its mipmaps; `levels[0]` is the full image, row by row."""
    width: int
    height: int
    levels: list = field(default_factory=list)   # lists of u16 pixels
    set_header: bytes = b""                       # bytes 0..15 of the entry, kept as read
    frame_tail: bytes = b""                       # frame header bytes 12..19, kept as read

    def level_sizes(self):
        sizes, w, h = [], self.width, self.height
        for _ in self.levels:
            sizes.append((w, h))
            w, h = max(1, w // 2), max(1, h // 2)
        return sizes


def parse_texture(payload):
    if len(payload) < 0x18 + FRAME_HEADER:
        raise FormatError("entry too short for a frame")
    word0, height, frames, _unknown = struct.unpack_from("<4I", payload, 0)
    width, mips = word0 & 0xFFFF, word0 >> 16
    if frames != 1:
        raise FormatError(f"{frames} frames; only single-frame textures are supported")
    frame_offset, frame_length = struct.unpack_from("<II", payload, 0x10)
    if frame_offset != 0x18 or frame_offset + frame_length != len(payload):
        raise FormatError("frame does not fill the entry after its table")
    flags, w, h = struct.unpack_from("<III", payload, frame_offset)
    if flags != FLAGS_RGB565:
        raise FormatError(f"frame flags {flags:#x}; only RGB565 ({FLAGS_RGB565:#x}) is supported")
    if (w, h) != (width, height) or not (0 < w <= 1024 and 0 < h <= 1024):
        raise FormatError("frame size disagrees with the entry header")
    rows_at = frame_offset + FRAME_HEADER
    rows = struct.unpack_from(f"<{h}I", payload, rows_at)
    first = FRAME_HEADER + 4 * h
    if rows != tuple(first + 2 * w * y for y in range(h)):
        raise FormatError("row offsets are not the plain layout")
    tex = Texture(width, height, [], payload[:0x10], payload[frame_offset + 12:frame_offset + FRAME_HEADER])
    cursor = rows_at + 4 * h
    lw, lh = w, h
    for _ in range(mips + 1):
        n = lw * lh
        if cursor + 2 * n > len(payload):
            raise FormatError("mipmap chain is cut short")
        tex.levels.append(list(struct.unpack_from(f"<{n}H", payload, cursor)))
        cursor += 2 * n
        lw, lh = max(1, lw // 2), max(1, lh // 2)
    if cursor != len(payload):
        raise FormatError(f"{len(payload) - cursor} unexplained bytes after the mipmaps")
    return tex


def build_texture(tex):
    w, h = tex.width, tex.height
    if len(tex.set_header) != 0x10 or len(tex.frame_tail) != 8:
        raise FormatError("texture lost its preserved header bytes")
    if struct.unpack_from("<I", tex.set_header, 0)[0] >> 16 != len(tex.levels) - 1:
        raise FormatError("mipmap count changed")
    for (lw, lh), level in zip(tex.level_sizes(), tex.levels):
        if len(level) != lw * lh or any(not 0 <= v <= 0xFFFF for v in level):
            raise FormatError("a level has the wrong size or a value outside 16 bits")
    rows = struct.pack(f"<{h}I", *(FRAME_HEADER + 4 * h + 2 * w * y for y in range(h)))
    pixels = b"".join(struct.pack(f"<{len(l)}H", *l) for l in tex.levels)
    frame = struct.pack("<III", FLAGS_RGB565, w, h) + tex.frame_tail + rows + pixels
    return tex.set_header + struct.pack("<II", 0x18, len(frame)) + frame


def unpack565(v):
    return (v >> 11) & 31, (v >> 5) & 63, v & 31


def pack565(r, g, b):
    return (r << 11) | (g << 5) | b


def to_rgb(v):
    r, g, b = unpack565(v)
    return (r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31


def from_rgb(r, g, b):
    return pack565((r * 31 + 127) // 255, (g * 63 + 127) // 255, (b * 31 + 127) // 255)


def downsample(level, w, h):
    """Next mipmap: 2x2 average per channel, rounded to nearest. The game's own rounding differs by at most one
    step per channel (CONFIRMED on entries 4472 and 4569)."""
    nw, nh = max(1, w // 2), max(1, h // 2)
    out = []
    for y in range(nh):
        for x in range(nw):
            quad = [unpack565(level[min(h - 1, 2 * y + dy) * w + min(w - 1, 2 * x + dx)]) for dy in (0, 1) for dx in (0, 1)]
            out.append(pack565(*((sum(q[k] for q in quad) + 2) // 4 for k in range(3))))
    return out


def with_image(tex, pixels):
    """A copy of `tex` whose full image is `pixels` (u16), with its mipmaps made again."""
    if len(pixels) != tex.width * tex.height:
        raise FormatError("image size does not match the texture")
    levels, w, h = [list(pixels)], tex.width, tex.height
    for _ in range(len(tex.levels) - 1):
        levels.append(downsample(levels[-1], w, h))
        w, h = max(1, w // 2), max(1, h // 2)
    return Texture(tex.width, tex.height, levels, tex.set_header, tex.frame_tail)


def patch_in_place(path, index, expected_sha256, payload):
    """Overwrite entry `index` with a payload of the same length, only when its current bytes hash to
    `expected_sha256`. Returns the bytes replaced. The table and every other byte stay as they are."""
    with open(path, "r+b") as f:
        offset, length = entry_location(f, index)
        if length != len(payload):
            raise FormatError(f"entry {index}: new payload is {len(payload)} bytes, slot holds {length}")
        f.seek(offset)
        old = f.read(length)
        if digest(old) != expected_sha256:
            raise FormatError(f"entry {index}: current bytes do not match the expected hash")
        f.seek(offset)
        f.write(payload)
    return old
