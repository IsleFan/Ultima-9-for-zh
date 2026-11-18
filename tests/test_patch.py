"""u9cht/l10n/patch.py on synthetic bytes (no game data)."""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from u9cht.l10n import patch  # noqa: E402

ENGLISH = "The quick brown fox jumps over the lazy dog near the old mill. " * 4


class PatchTests(unittest.TestCase):
    def test_round_trip_keeps_original_text_out(self):
        source = b"HEAD" + ENGLISH.encode("utf-16-le") + b"\x00\x00" + b"plain ascii passage of gargish text here" + b"TAIL"
        target = (b"HEAD" + "你好，聖者。".encode("utf-16-le") + ENGLISH.encode("utf-16-le") +
                  "plain ascii passage of gargish text here".encode("utf-16-le") + b"TAIL!")
        p = patch.make(source, target)
        self.assertEqual(patch.apply(source, p), target)
        self.assertEqual(patch.latin_runs(p), [], "kept English and widened ASCII come from the player's file")
        self.assertNotIn(b"quick", p)
        self.assertNotIn(b"g\x00a\x00r\x00g\x00i\x00s\x00h", p)
        kinds = set()
        count = struct.unpack_from("<I", p, 16)[0]
        at = 20
        for _ in range(count):
            kinds.add(p[at])
            at += 9 if p[at] in (0, 2) else 5 + struct.unpack_from("<I", p, at + 1)[0]
        self.assertEqual(kinds, {0, 1, 2})

    def test_new_text_is_added(self):
        p = patch.make(b"abc", "全新的中文內容".encode("utf-16-le"))
        self.assertEqual(patch.apply(b"abc", p), "全新的中文內容".encode("utf-16-le"))

    def test_refusals(self):
        p = patch.make(b"0123456789abcdef", b"0123456789abcdef")
        with self.assertRaises(patch.PatchError):
            patch.apply(b"0123456789abcde", p)
        with self.assertRaises(patch.PatchError):
            patch.apply(b"0123456789abcdef", b"XXXX" + p[4:])
        with self.assertRaises(patch.PatchError):
            patch.apply(b"0123456789abcdef", p + b"\x00")
        bad = bytearray(p)
        struct.pack_into("<I", bad, 21, 15)   # copy offset past the end
        with self.assertRaises(patch.PatchError):
            patch.apply(b"0123456789abcdef", bytes(bad))

    def test_latin_runs_flags_added_english(self):
        p = patch.make(b"", ENGLISH.encode("utf-16-le"))
        self.assertTrue(patch.latin_runs(p))


if __name__ == "__main__":
    unittest.main()
