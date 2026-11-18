"""Automatic review of the translation stores (LOCAL-ONLY report).

Three checks the human asked for (2026-11-18), plus a few cheap ones:

1. Same source, different translations: identical English source text translated more than one way.
2. Proper nouns off the authority file: an approved person/place/... term in the source whose approved Chinese is
   absent from the translation, grouped by term.
3. Overlong text: dialogue lines past the layout budget or with more lines than the screen was seen to hold, and
   dialogue far longer or far shorter (possibly cut) than its source. On 2026-11-18 the ratio of Chinese characters
   to English characters had median 0.27, 99th percentile 0.41, maximum 0.52 (6,729 lines of 20+ characters).
4. Also: Latin words left in a translation, half-width punctuation between Chinese characters, doubled characters,
   inconsistent ellipses.

The report quotes English source text, so it is written under .local/ only and must never be committed.

    python scripts/qa_review.py --game-dir .local/runtime-game --output .local/qa/<date>/review.md
"""
from __future__ import annotations

import argparse
import collections
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from u9cht.l10n.adapters import ADAPTERS  # noqa: E402
from u9cht.l10n.glossary import contains_standalone, declared_not_terms, parse_glossary  # noqa: E402
from u9cht.l10n.pipeline import Paths, load_json, read_game  # noqa: E402
from u9cht.l10n.store import load_store  # noqa: E402

PROPER = {"person", "place", "organization", "virtue", "monster", "spell", "title"}
CJK = r"㐀-鿿豈-﫿"
HALF_PUNCT = re.compile(rf"(?<=[{CJK}])[,?!:;](?=[{CJK}\s]|$)|(?<=[{CJK}])\.(?=[{CJK}]|$)")
DOUBLED = re.compile(r"(的的|了了|是是|在在|。。|，，|？？(?!？)|！！！+)")
LATIN_WORD = re.compile(r"[A-Za-z][A-Za-z'\-]{2,}")
PLACEHOLDER = re.compile(r"\{\{[^}]*\}\}|%[sd]")
LONG_RATIO, SHORT_RATIO, MIN_SOURCE = 0.45, 0.13, 25


def norm_source(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def latin_outside_annotations(target: str) -> list[str]:
    """Latin words that are not inside a (...) / （...） annotation or a placeholder."""
    stripped = PLACEHOLDER.sub(" ", target)
    stripped = re.sub(r"[（(][^）)]*[）)]", " ", stripped)
    return LATIN_WORD.findall(stripped)


def cells(line: str) -> int:
    """Approximate width in 16-unit cells: CJK and full-width = 1, ASCII = 1 (the layout check's estimate)."""
    return len(line)


def review(records, rows, sources, profiles):
    out = collections.OrderedDict()
    active = [r for r in records if r["status"] not in ("untranslated", "blocked") and r.get("target")]

    # 1. same source, different translations
    groups = collections.defaultdict(list)
    for r in active:
        src = sources.get(r["key"])
        if src and len(norm_source(src)) >= 2:
            groups[norm_source(src)].append(r)
    same = []
    for src, rs in groups.items():
        targets = collections.Counter(r["target"] for r in rs)
        if len(targets) > 1:
            same.append((src, rs, targets))
    same.sort(key=lambda x: (-len(x[1]), x[0]))
    out["same_source"] = same

    # 2. proper nouns off the authority file
    approved = {}
    for row in rows:
        if row["status"] == "approved" and row["category"] in PROPER and row["target"] != row["source"]:
            approved.setdefault(row["source"], row)
    vocabulary = [r["source"] for r in rows if r["status"] in ("approved", "keep")]
    others = collections.defaultdict(set)   # term -> non-approved targets listed for it
    for row in rows:
        if row["source"] in approved and row["target"] and row["target"] != approved[row["source"]]["target"]:
            others[row["source"]].add(row["target"])
    off = collections.defaultdict(list)
    for r in active:
        src = sources.get(r["key"])
        if not src:
            continue
        skip = declared_not_terms(r)
        for term, row in approved.items():
            if term in skip or term not in src:
                continue
            if contains_standalone(src, term, vocabulary) and row["target"] not in r["target"]:
                variants = sorted(v for v in others.get(term, ()) if v in r["target"])
                off[term].append((r, variants))
    out["proper"] = sorted(off.items(), key=lambda kv: (-len(kv[1]), kv[0]))
    out["proper_rows"] = approved

    # 3. overlong
    budget = {}
    for p in profiles:
        for c in p["categories"]:
            budget[(p["resource"], c)] = p
    long_lines, many_lines, verbose, terse = [], [], [], []
    for r in active:
        p = budget.get((r["resource"], r.get("category")))
        lines = r["target"].split("\n")
        if p:
            limit = p["budget_logical"] // 16
            widest = max(cells(x) for x in lines)
            if widest > limit:
                long_lines.append((r, widest, limit))
            if p.get("observed_max_lines") and len(lines) > p["observed_max_lines"]:
                many_lines.append((r, len(lines), p["observed_max_lines"]))
        src = sources.get(r["key"])
        if src and r["resource"] == "text.flx":
            zh = len(re.findall(rf"[{CJK}]", r["target"]))
            en = len(norm_source(src))
            if en >= MIN_SOURCE and zh > LONG_RATIO * en:
                verbose.append((r, zh, en))
            elif en >= MIN_SOURCE and zh < SHORT_RATIO * en:
                terse.append((r, zh, en))
    out["long_lines"] = long_lines
    out["many_lines"] = sorted(many_lines, key=lambda x: -x[1])
    out["verbose"] = sorted(verbose, key=lambda x: -(x[1] / x[2]))
    out["terse"] = sorted(terse, key=lambda x: x[1] / x[2])

    # 4. small things
    latin, punct, doubled, ellipsis = [], [], [], collections.Counter()
    for r in active:
        t = r["target"]
        words = latin_outside_annotations(t)
        if words and r["resource"] != "misctext.flx":
            latin.append((r, words))
        if HALF_PUNCT.search(t):
            punct.append(r)
        m = DOUBLED.search(t)
        if m:
            doubled.append((r, m.group(0)))
        for form in ("……", "...", "…", "。。。"):
            if form in t:
                ellipsis[form] += 1
                t = t.replace(form, "")
    out["latin"] = latin
    out["punct"] = punct
    out["doubled"] = doubled
    out["ellipsis"] = ellipsis
    return out


def clip(text: str, n: int = 60) -> str:
    text = text.replace("\n", "⏎")
    return text if len(text) <= n else text[:n] + "…"


def render(result, sources, limit) -> str:
    md = ["# 譯文自動檢查（LOCAL-ONLY：含英文原文，不可提交）", ""]
    same, proper = result["same_source"], result["proper"]
    md += ["## 摘要", "",
           "| 項目 | 筆數 |", "| --- | --- |",
           f"| 1. 同一原文、不同譯法（組） | {len(same)} |",
           f"| 2. 專有名詞沒照權威檔（詞 / 筆） | {len(proper)} / {sum(len(v) for _, v in proper)} |",
           f"| 3a. 對白單行超過版面 | {len(result['long_lines'])} |",
           f"| 3b. 行數超過畫面看過的最多行數 | {len(result['many_lines'])} |",
           f"| 3c. 對白明顯比原文長（中文字數 > {LONG_RATIO} × 英文字元） | {len(result['verbose'])} |",
           f"| 3d. 對白明顯比原文短，可能漏譯（< {SHORT_RATIO} ×） | {len(result['terse'])} |",
           f"| 4a. 譯文裡留下的英文字（括號加註以外） | {len(result['latin'])} |",
           f"| 4b. 中文之間的半形標點 | {len(result['punct'])} |",
           f"| 4c. 重複字或標點 | {len(result['doubled'])} |",
           "", "刪節號用法：" + "、".join(f"`{k}` {v} 筆" for k, v in result["ellipsis"].most_common()), ""]

    md += ["## 1. 同一原文、不同譯法", "",
           "同一句英文（空白正規化後完全相同）出現多種中文。有些是刻意的（上下文不同），請挑著看。", ""]
    for src, rs, targets in same[:limit]:
        md.append(f"- **{clip(src, 80)}**（{len(rs)} 筆）")
        for t, n in targets.most_common():
            keys = [r["key"] for r in rs if r["target"] == t]
            md.append(f"  - {clip(t, 70)} ×{n}：{', '.join(keys[:4])}{' …' if len(keys) > 4 else ''}")
    if len(same) > limit:
        md.append(f"- …另有 {len(same) - limit} 組")
    md.append("")

    md += ["## 2. 專有名詞沒照權威檔", "",
           "原文出現權威檔已核准的專有名詞（人名、地名、組織、美德、怪物、法術、稱號），譯文裡卻沒有核准譯名。"
           "代名詞、省略或刻意改寫也會被列出，請挑著看。「用了」表示譯文裡出現權威檔列為非核准的譯法。", ""]
    rows = result["proper_rows"]
    for term, items in proper[:limit]:
        md.append(f"- **{term}** → 核准：{rows[term]['target']}（{rows[term]['category']}，{len(items)} 筆）")
        for r, variants in items[:5]:
            used = f"　用了：{'、'.join(variants)}" if variants else ""
            md.append(f"  - `{r['key']}` {clip(r['target'], 60)}{used}")
        if len(items) > 5:
            md.append(f"  - …另有 {len(items) - 5} 筆")
    md.append("")

    md += ["## 3. 過長", "", "### 3a. 對白單行超過版面（格數估計：每字 1 格）", ""]
    for r, w, lim in result["long_lines"][:limit]:
        md.append(f"- `{r['key']}` {w} 格 > {lim}：{clip(r['target'], 70)}")
    md += ["", "### 3b. 行數超過畫面看過的最多行數", ""]
    for r, n, mx in result["many_lines"][:limit]:
        md.append(f"- `{r['key']}`（{r.get('category')}）{n} 行 > {mx}：{clip(r['target'], 70)}")
    md += ["", "### 3c. 對白明顯比原文長", "",
           f"中文字數超過英文字元數的 {LONG_RATIO} 倍（中位數 0.27）。可能補了太多說明。", ""]
    for r, zh, en in result["verbose"][:limit]:
        md.append(f"- `{r['key']}` 中 {zh} / 英 {en}：{clip(r['target'], 50)}　｜原文：{clip(sources.get(r['key'], ''), 50)}")
    md += ["", "### 3d. 對白明顯比原文短（可能漏譯）", "",
           f"中文字數不到英文字元數的 {SHORT_RATIO} 倍。祈禱詞、石像鬼語、按鍵說明這類保留英文的句子也會出現在這裡。", ""]
    for r, zh, en in result["terse"][:limit]:
        md.append(f"- `{r['key']}` 中 {zh} / 英 {en}：{clip(r['target'], 50)}　｜原文：{clip(sources.get(r['key'], ''), 70)}")
    md.append("")

    md += ["## 4. 其他", "", "### 4a. 譯文裡留下的英文字（括號加註以外；不含介面字串）", ""]
    for r, words in result["latin"][:limit]:
        md.append(f"- `{r['key']}` {', '.join(sorted(set(words))[:5])}：{clip(r['target'], 60)}")
    md += ["", "### 4b. 中文之間的半形標點", ""]
    for r in result["punct"][:limit]:
        md.append(f"- `{r['key']}` {clip(r['target'], 70)}")
    md += ["", "### 4c. 重複字或標點", ""]
    for r, what in result["doubled"][:limit]:
        md.append(f"- `{r['key']}` 「{what}」：{clip(r['target'], 60)}")
    md.append("")
    return "\n".join(md)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--game-dir", required=True, help="private copy with the original resources (LOCAL-ONLY)")
    ap.add_argument("--output", required=True, help="Markdown report path under .local/")
    ap.add_argument("--limit", type=int, default=200, help="items listed per section")
    args = ap.parse_args(argv)
    output = Path(args.output).resolve()
    if ".local" not in output.parts:
        raise SystemExit("STOP: the report quotes English source text; write it under .local/")
    paths = Paths()
    records, issues, _ = load_store(paths.store)
    rows, gloss = parse_glossary(paths.glossary.read_bytes())
    game = read_game(Path(args.game_dir), load_json(paths.sources), issues)
    errors = [i for i in issues if i.level == "error"]
    if errors:
        for i in errors[:20]:
            print(f"ERROR {i.code} {i.key}: {i.message}")
        return 1
    sources = {}
    for resource, data in game.items():
        units = ADAPTERS[resource].units(data)
        for r in records:
            if r["resource"] == resource:
                unit = units.get(r["entry_id"])
                if unit is not None and unit.text is not None and unit.sha256 == r["source_sha256"]:
                    sources[r["key"]] = unit.text
    profiles = json.loads((ROOT / "translation" / "resource-capabilities.json").read_text(encoding="utf-8"))["layout_profiles"]
    result = review(records, rows, sources, profiles)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(render(result, sources, args.limit), encoding="utf-8")
    print(f"report: {output}")
    print(f"same-source groups {len(result['same_source'])}; proper-noun terms {len(result['proper'])} "
          f"({sum(len(v) for _, v in result['proper'])} records); long lines {len(result['long_lines'])}; "
          f"many lines {len(result['many_lines'])}; verbose {len(result['verbose'])}; terse {len(result['terse'])}; latin {len(result['latin'])}; "
          f"half-width punctuation {len(result['punct'])}; doubled {len(result['doubled'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
