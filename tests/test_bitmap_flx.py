"""bitmap16.flx texture reader/writer on synthetic files (no game assets)."""

import os
import struct
import tempfile
import unittest
from pathlib import Path

from u9cht import bitmap_flx as bf


def synthetic_payload(w=8, h=4, mips=2, seed=1):
    levels, lw, lh, v = [], w, h, seed
    for _ in range(mips + 1):
        level = []
        for _ in range(lw * lh):
            v = (v * 1103515245 + 12345) & 0xFFFF
            level.append(v)
        levels.append(level)
        lw, lh = max(1, lw // 2), max(1, lh // 2)
    tex = bf.Texture(w, h, levels, struct.pack("<4I", w | (mips << 16), h, 1, 0x56000), b"\0" * 8)
    return bf.build_texture(tex)


def synthetic_flx(payloads, slots=6):
    header = bytearray(b" " * 0x4C) + bytearray(0x34)
    struct.pack_into("<II", header, 0x50, slots, 2)
    table = bytearray(8 * slots)
    body, cursor = b"", 0x80 + 8 * slots
    for index, payload in payloads.items():
        struct.pack_into("<II", table, 8 * index, cursor + len(body), len(payload))
        body += payload
    data = bytes(header) + bytes(table) + body
    struct.pack_into("<II", header, 0x58, len(data), len(data))
    return bytes(header) + bytes(table) + body


class BitmapFlxTests(unittest.TestCase):
    def test_round_trip_is_byte_perfect(self):
        payload = synthetic_payload()
        tex = bf.parse_texture(payload)
        self.assertEqual((tex.width, tex.height, len(tex.levels)), (8, 4, 3))
        self.assertEqual(bf.build_texture(tex), payload)

    def test_layout_matches_the_signposts(self):
        payload = synthetic_payload(64, 32, 3)
        self.assertEqual(len(payload), 5612)   # the size of entries 4471..4478, 4568, 4569
        self.assertEqual(struct.unpack_from("<I", payload, 0x18 + 0x14)[0], 0x94)

    def test_new_image_rebuilds_mipmaps(self):
        tex = bf.parse_texture(synthetic_payload())
        white = bf.pack565(31, 63, 31)
        new = bf.with_image(tex, [white] * 32)
        self.assertEqual(new.levels[1], [white] * 8)
        self.assertEqual(len(bf.build_texture(new)), len(synthetic_payload()))

    def test_color_conversion(self):
        for v in (0, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0x1234):
            self.assertEqual(bf.from_rgb(*bf.to_rgb(v)), v)

    def test_refuses_other_layouts(self):
        payload = bytearray(synthetic_payload())
        struct.pack_into("<I", payload, 0x18, 0x60000500)
        with self.assertRaises(bf.FormatError):
            bf.parse_texture(bytes(payload))
        with self.assertRaises(bf.FormatError):
            bf.parse_texture(synthetic_payload()[:-2])
        with self.assertRaises(bf.FormatError):
            bf.parse_texture(synthetic_payload() + b"\0\0")
        bad_rows = bytearray(synthetic_payload())
        struct.pack_into("<I", bad_rows, 0x18 + 0x14, 0)
        with self.assertRaises(bf.FormatError):
            bf.parse_texture(bytes(bad_rows))
        two_frames = bytearray(synthetic_payload())
        struct.pack_into("<I", two_frames, 8, 2)
        with self.assertRaises(bf.FormatError):
            bf.parse_texture(bytes(two_frames))

    def test_patch_in_place(self):
        a, b = synthetic_payload(seed=1), synthetic_payload(seed=2)
        data = synthetic_flx({1: a, 3: b})
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "bitmap16.flx"
            path.write_bytes(data)
            self.assertEqual(bf.read_entry(path, 3), b)
            new = synthetic_payload(seed=3)
            with self.assertRaises(bf.FormatError):
                bf.patch_in_place(path, 3, bf.digest(a), new)        # wrong expected hash
            with self.assertRaises(bf.FormatError):
                bf.patch_in_place(path, 3, bf.digest(b), new + b"x")  # size change
            self.assertEqual(path.read_bytes(), data)
            old = bf.patch_in_place(path, 3, bf.digest(b), new)
            self.assertEqual(old, b)
            after = path.read_bytes()
            self.assertEqual(len(after), len(data))
            self.assertEqual(bf.read_entry(path, 3), new)
            self.assertEqual(bf.read_entry(path, 1), a)
            bf.patch_in_place(path, 3, bf.digest(new), old)
            self.assertEqual(path.read_bytes(), data)
            with self.assertRaises(bf.FormatError):
                bf.read_entry(path, 0)                                # empty slot
            with self.assertRaises(bf.FormatError):
                bf.read_entry(path, 99)                               # outside the table

    @unittest.skipUnless(os.environ.get("U9_GAME_DIR"), "LOCAL-ONLY: set U9_GAME_DIR")
    def test_local_signposts_round_trip(self):
        path = Path(os.environ["U9_GAME_DIR"]) / "static" / "bitmap16.flx"
        for index in (4471, 4472, 4473, 4474, 4475, 4476, 4477, 4478, 4568, 4569):
            payload = bf.read_entry(path, index)
            self.assertEqual(bf.build_texture(bf.parse_texture(payload)), payload, index)


if __name__ == "__main__":
    unittest.main()
