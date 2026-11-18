"""Release patches: original FLX -> Chinese FLX without shipping the original's bytes.

Format (docs/release/PATCH_FORMAT.md, applied by runtime/common/flx_patch.h):
  b"U9CP" | u32 version | u32 source size | u32 target size | u32 op count
  op 0 (copy): u8 0, u32 source offset, u32 length
  op 1 (add):  u8 1, u32 length, bytes
  op 2 (widen): u8 2, u32 source offset, u32 length; each source byte becomes a UTF-16LE unit (byte, 0)
Every run of at least MIN_COPY bytes that the original already holds is copied from the player's own file, so
the English text the build leaves unchanged never enters the patch.
"""
import re
import struct

MAGIC = b"U9CP"
VERSION = 1
MIN_COPY = 8
_KEY = 8


class PatchError(ValueError):
    pass


def make(source: bytes, target: bytes) -> bytes:
    index = {}
    for i in range(len(source) - _KEY + 1):
        index.setdefault(source[i:i + _KEY], []).append(i)
    ops, add = [], bytearray()
    i, hint = 0, 0

    def flush():
        if add:
            ops.append((1, bytes(add)))
            add.clear()

    while i < len(target):
        best_len, best_at = 0, 0
        candidates = index.get(target[i:i + _KEY], ())
        # Try the continuation of the previous copy first, then a few others.
        tries = ([hint] if hint in candidates else []) + list(candidates[:16])
        for at in tries:
            n = 0
            limit = min(len(source) - at, len(target) - i)
            while n < limit and source[at + n] == target[i + n]:
                n += 1
            if n > best_len:
                best_len, best_at = n, at
        # The build re-encodes some original ASCII passages (the Gargish quoted in books) as UTF-16LE.
        wide_len, wide_at = 0, 0
        if best_len < MIN_COPY * 2 and i + 2 * _KEY <= len(target) and not any(target[i + 1:i + 2 * _KEY:2]):
            for at in index.get(target[i:i + 2 * _KEY:2], ())[:16]:
                n = 0
                limit = min(len(source) - at, (len(target) - i) // 2)
                while n < limit and target[i + 2 * n] == source[at + n] and target[i + 2 * n + 1] == 0:
                    n += 1
                if n > wide_len:
                    wide_len, wide_at = n, at
        if wide_len >= MIN_COPY and 2 * wide_len > best_len:
            flush()
            ops.append((2, wide_at, wide_len))
            i += 2 * wide_len
        elif best_len >= MIN_COPY:
            flush()
            ops.append((0, best_at, best_len))
            i += best_len
            hint = best_at + best_len
        else:
            add.append(target[i])
            i += 1
    flush()
    out = bytearray(MAGIC + struct.pack("<IIII", VERSION, len(source), len(target), len(ops)))
    for op in ops:
        if op[0] in (0, 2):
            out += struct.pack("<BII", op[0], op[1], op[2])
        else:
            out += struct.pack("<BI", 1, len(op[1])) + op[1]
    return bytes(out)


def apply(source: bytes, patch: bytes) -> bytes:
    if patch[:4] != MAGIC:
        raise PatchError("not a U9CP patch")
    if len(patch) < 20:
        raise PatchError("truncated header")
    version, source_size, target_size, count = struct.unpack_from("<IIII", patch, 4)
    if version != VERSION:
        raise PatchError("unsupported patch version")
    if source_size != len(source):
        raise PatchError("source size differs")
    at, out = 20, bytearray()
    for _ in range(count):
        if at >= len(patch):
            raise PatchError("truncated op")
        kind = patch[at]
        if kind == 0:
            if len(patch) - at < 9:
                raise PatchError("truncated copy")
            offset, length = struct.unpack_from("<II", patch, at + 1)
            if offset + length > len(source):
                raise PatchError("copy outside the source")
            out += source[offset:offset + length]
            at += 9
        elif kind == 2:
            if len(patch) - at < 9:
                raise PatchError("truncated widen")
            offset, length = struct.unpack_from("<II", patch, at + 1)
            if offset + length > len(source):
                raise PatchError("widen outside the source")
            for b in source[offset:offset + length]:
                out += bytes((b, 0))
            at += 9
        elif kind == 1:
            if len(patch) - at < 5:
                raise PatchError("truncated add")
            (length,) = struct.unpack_from("<I", patch, at + 1)
            at += 5
            if at + length > len(patch):
                raise PatchError("add runs past the patch")
            out += patch[at:at + length]
            at += length
        else:
            raise PatchError("unknown op")
        if len(out) > target_size:
            raise PatchError("output too long")
    if at != len(patch):
        raise PatchError("trailing bytes")
    if len(out) != target_size:
        raise PatchError("output size differs")
    return bytes(out)


def added_runs(patch: bytes):
    """The add payloads of a patch, in order (for the release leak check)."""
    count = struct.unpack_from("<I", patch, 16)[0]
    at, runs = 20, []
    for _ in range(count):
        if patch[at] in (0, 2):
            at += 9
        else:
            (length,) = struct.unpack_from("<I", patch, at + 1)
            runs.append(patch[at + 5:at + 5 + length])
            at += 5 + length
    return runs


_LATIN_UTF16 = re.compile(rb"(?:[A-Za-z ,.'!?;:\-]\x00){16,}")
_LATIN_ASCII = re.compile(rb"[A-Za-z ,.'!?;:\-]{16,}")


def latin_runs(patch: bytes):
    """Runs of 16 or more Latin characters (UTF-16LE or ASCII) inside the added bytes."""
    found = []
    for run in added_runs(patch):
        found += [m.group().decode("utf-16-le") for m in _LATIN_UTF16.finditer(run)]
        found += [m.group().decode("ascii") for m in _LATIN_ASCII.finditer(run)]
    return found
