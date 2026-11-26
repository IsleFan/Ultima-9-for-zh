"""bitmap16.flx textures: read one entry, decode and rebuild its 16-bit frame, patch it in place.

Only the layout the signpost textures use is supported (docs/formats/U9-BITMAP16-NOTES.md):
one frame, RGB565 (frame flags 0x60000400), followed by its mipmaps. Anything else is refused,
so unknown bytes are never regenerated. The file is read by offset; it is never loaded whole
(bitmap16.flx is about 320 MB).
"""

import hashlib
from pathlib import Path
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


# ---- signpost overlays (U9CS; docs/formats/U9-BITMAP16-NOTES.md, "Release patch") ----

SIGN_MAGIC, SIGN_VERSION = b"U9CS", 1


def read_png_rgba(data):
    """Pixels of an 8-bit RGBA, non-interlaced PNG as a list of (r, g, b, a). Standard library only."""
    import zlib
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise FormatError("not a PNG")
    at, idat, w = 8, b"", None
    while at < len(data):
        length = struct.unpack_from(">I", data, at)[0]
        kind, body = data[at + 4:at + 8], data[at + 8:at + 8 + length]
        if kind == b"IHDR":
            w, h, depth, color, _c, _f, interlace = struct.unpack(">IIBBBBB", body)
            if (depth, color, interlace) != (8, 6, 0):
                raise FormatError("only 8-bit RGBA PNGs without interlace are read")
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
        at += 12 + length
    if w is None:
        raise FormatError("PNG has no header")
    raw, stride, prev, out = zlib.decompress(idat), w * 4, bytearray(w * 4), []
    for y in range(h):
        kind, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - 4] if i >= 4 else 0
            b, c = prev[i], prev[i - 4] if i >= 4 else 0
            if kind == 1: line[i] = (line[i] + a) & 255
            elif kind == 2: line[i] = (line[i] + b) & 255
            elif kind == 3: line[i] = (line[i] + (a + b) // 2) & 255
            elif kind == 4:
                p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
            elif kind != 0: raise FormatError("bad PNG filter")
        out += [tuple(line[i:i + 4]) for i in range(0, stride, 4)]
        prev = line
    return w, h, out


def overlay(base_pixels, layer):
    """`layer` (RGBA) laid over RGB565 pixels: each channel (L*a + B*(255-a) + 127) // 255 in 8-bit RGB, then back
    to RGB565. A pixel the layer leaves transparent keeps its exact value. The front page does the same
    (runtime/common/sign_patch.h)."""
    out = []
    for v, (r, g, b, a) in zip(base_pixels, layer):
        if a == 0:
            out.append(v); continue
        br, bg, bb = to_rgb(v)
        out.append(from_rgb(*((s * a + d * (255 - a) + 127) // 255 for s, d in ((r, br), (g, bg), (b, bb)))))
    return out


def sign_payload(base_payload, target_payload, layer):
    base, target = parse_texture(base_payload), parse_texture(target_payload)
    if (base.width, base.height, len(base.levels)) != (target.width, target.height, len(target.levels)):
        raise FormatError("base and target boards differ in size")
    if len(layer) != target.width * target.height:
        raise FormatError("layer size does not match the board")
    payload = build_texture(with_image(target, overlay(base.levels[0], layer)))
    if len(payload) != len(target_payload):
        raise FormatError("board changed size")
    return payload


def file_sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 22), b""):
            h.update(block)
    return h.hexdigest()


def make_sign_patch(source_path, base_entry, layers):
    """U9CS bytes for `layers` ({entry: (width, height, rgba pixels)}) over `base_entry`, with every hash the front
    page checks, computed from the developer's own copy of the original file."""
    size = Path(source_path).stat().st_size
    base_payload = read_entry(source_path, base_entry)
    out = bytearray(SIGN_MAGIC + struct.pack("<II", SIGN_VERSION, size) + bytes.fromhex(file_sha256(source_path)))
    out += struct.pack("<I", base_entry) + bytes.fromhex(digest(base_payload)) + struct.pack("<I", len(layers))
    payloads = {}
    for entry in sorted(layers):
        w, h, pixels = layers[entry]
        original = read_entry(source_path, entry)
        payload = sign_payload(base_payload, original, pixels)
        payloads[entry] = payload
        out += struct.pack("<IHH", entry, w, h) + bytes.fromhex(digest(original))
        out += b"".join(bytes(p) for p in pixels) + bytes.fromhex(digest(payload))
    h = hashlib.sha256()
    with open(source_path, "rb") as f:
        offsets = {e: entry_location(f, e) for e in payloads}
        cursor = 0
        for entry, (offset, length) in sorted(offsets.items(), key=lambda kv: kv[1][0]):
            f.seek(cursor)
            remaining = offset - cursor
            while remaining:
                block = f.read(min(remaining, 1 << 22)); h.update(block); remaining -= len(block)
            h.update(payloads[entry]); cursor = offset + length
        f.seek(cursor)
        for block in iter(lambda: f.read(1 << 22), b""):
            h.update(block)
    return bytes(out + bytes.fromhex(h.hexdigest()))


@dataclass
class SignPatch:
    source_size: int
    source_sha256: str
    base_entry: int
    base_sha256: str
    signs: list            # (entry, width, height, original sha, rgba pixels, result sha)
    result_sha256: str


def parse_sign_patch(data):
    if data[:4] != SIGN_MAGIC or len(data) < 4 + 8 + 32 + 4 + 32 + 4 + 32:
        raise FormatError("not a U9CS patch")
    version, size = struct.unpack_from("<II", data, 4)
    if version != SIGN_VERSION:
        raise FormatError(f"U9CS version {version}")
    at = 12; source = data[at:at + 32].hex(); at += 32
    base_entry = struct.unpack_from("<I", data, at)[0]; at += 4
    base_sha = data[at:at + 32].hex(); at += 32
    count = struct.unpack_from("<I", data, at)[0]; at += 4
    if count > 256:
        raise FormatError("too many boards")
    signs = []
    for _ in range(count):
        if at + 8 + 32 > len(data):
            raise FormatError("U9CS cut short")
        entry, w, h = struct.unpack_from("<IHH", data, at); at += 8
        original = data[at:at + 32].hex(); at += 32
        n = w * h * 4
        if not (0 < w <= 256 and 0 < h <= 256) or at + n + 32 > len(data):
            raise FormatError("U9CS board cut short")
        pixels = [tuple(data[i:i + 4]) for i in range(at, at + n, 4)]; at += n
        signs.append((entry, w, h, original, pixels, data[at:at + 32].hex())); at += 32
    if at + 32 != len(data):
        raise FormatError("U9CS has the wrong length")
    return SignPatch(size, source, base_entry, base_sha, signs, data[at:].hex())


def apply_sign_patch(source_path, patch_bytes, out_path):
    """Reference for the front page: copy the player's file to `out_path` and write the boards in place, checking
    every hash of the patch. Returns the result SHA-256."""
    import shutil
    p = parse_sign_patch(patch_bytes)
    if Path(source_path).stat().st_size != p.source_size or file_sha256(source_path) != p.source_sha256:
        raise FormatError("the game's bitmap16.flx is not the file the patch was made for")
    base = read_entry(source_path, p.base_entry)
    if digest(base) != p.base_sha256:
        raise FormatError("blank board does not match")
    payloads = []
    for entry, w, h, original_sha, pixels, result_sha in p.signs:
        original = read_entry(source_path, entry)
        if digest(original) != original_sha:
            raise FormatError(f"board {entry} does not match")
        payload = sign_payload(base, original, pixels)
        if digest(payload) != result_sha:
            raise FormatError(f"board {entry} came out different")
        payloads.append((entry, original_sha, payload))
    shutil.copyfile(source_path, out_path)
    for entry, original_sha, payload in payloads:
        patch_in_place(out_path, entry, original_sha, payload)
    result = file_sha256(out_path)
    if result != p.result_sha256:
        raise FormatError("result does not match the patch")
    return result
