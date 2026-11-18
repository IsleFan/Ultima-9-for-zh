"""scripts/qa_review.py on synthetic records (no game data)."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import qa_review as q  # noqa: E402


def record(key, target, category="dialogue", resource="text.flx", notes=None):
    return {"key": key, "resource": resource, "entry_id": int(key.split(":")[1]), "status": "draft",
            "category": category, "target": target, "notes": notes}


def term(source, target, category="person", status="approved"):
    return {"source": source, "target": target, "category": category, "status": status}


PROFILES = [{"resource": "text.flx", "categories": ["dialogue"], "budget_logical": 64, "observed_max_lines": 2}]


class QaReviewTests(unittest.TestCase):
    def run_review(self, records, rows, sources):
        return q.review(records, rows, sources, PROFILES)

    def test_same_source_different_targets(self):
        recs = [record("text.flx:1", "再會。"), record("text.flx:2", "再見。"), record("text.flx:3", "再會。"),
                record("text.flx:4", "你好。")]
        src = {"text.flx:1": "Farewell.", "text.flx:2": "Farewell. ", "text.flx:3": "Farewell.", "text.flx:4": "Hello."}
        same = self.run_review(recs, [], src)["same_source"]
        self.assertEqual(len(same), 1)
        text, rs, targets = same[0]
        self.assertEqual(text, "Farewell.")
        self.assertEqual(targets, {"再會。": 2, "再見。": 1})

    def test_proper_noun_missing_and_variant(self):
        rows = [term("Iolo", "伊歐洛"), term("Iolo", "約羅", status="todo"), term("Gold", "黃金", category="item"),
                term("Shrine", "神殿", category="place"), term("Shrine of Honor", "榮譽神殿", category="place")]
        recs = [record("text.flx:1", "約羅來了。"), record("text.flx:2", "伊歐洛來了。"), record("text.flx:3", "給你金幣。"),
                record("text.flx:4", "去榮譽神殿。"), record("text.flx:5", "他走了。", notes="not a term: Iolo")]
        src = {"text.flx:1": "Iolo is here.", "text.flx:2": "Iolo is here.", "text.flx:3": "Take the Gold.",
               "text.flx:4": "Go to the Shrine of Honor.", "text.flx:5": "Iolo left."}
        proper = dict(self.run_review(recs, rows, src)["proper"])
        self.assertEqual(list(proper), ["Iolo"], "items are not proper nouns; a longer term covers its part")
        (r, variants), = proper["Iolo"]
        self.assertEqual((r["key"], variants), ("text.flx:1", ["約羅"]))

    def test_length_checks(self):
        recs = [record("text.flx:1", "一二三四五\n六\n七"), record("text.flx:2", "一二三四五"),
                record("text.flx:3", "站住！"), record("text.flx:4", "一" * 20)]
        src = {"text.flx:1": "x", "text.flx:2": "y", "text.flx:3": "Stop right where you are, stranger!",
               "text.flx:4": "This sentence has exactly forty chars.."}
        out = self.run_review(recs, [], src)
        self.assertEqual(sorted((r["key"], w, lim) for r, w, lim in out["long_lines"]),
                         [("text.flx:1", 5, 4), ("text.flx:2", 5, 4), ("text.flx:4", 20, 4)], "64 units = 4 cells")
        self.assertEqual([(r["key"], n) for r, n, _ in out["many_lines"]], [("text.flx:1", 3)])
        self.assertEqual([r["key"] for r, _, _ in out["terse"]], ["text.flx:3"])
        self.assertEqual([r["key"] for r, _, _ in out["verbose"]], ["text.flx:4"])

    def test_small_things(self):
        recs = [record("text.flx:1", "說出「Ahm」(Mantra)。"), record("text.flx:2", "好,走吧。"),
                record("text.flx:3", "我的的書……"), record("misctext.flx:4", "Game", category="ui", resource="misctext.flx")]
        out = self.run_review(recs, [], {})
        self.assertEqual([(r["key"], w) for r, w in out["latin"]], [("text.flx:1", ["Ahm"])])
        self.assertEqual([r["key"] for r in out["punct"]], ["text.flx:2"])
        self.assertEqual([(r["key"], w) for r, w in out["doubled"]], [("text.flx:3", "的的")])
        self.assertEqual(out["ellipsis"]["……"], 1)

    def test_report_refuses_tracked_paths(self):
        with self.assertRaises(SystemExit):
            q.main(["--game-dir", "nowhere", "--output", "reports/review.md"])


if __name__ == "__main__":
    unittest.main()
