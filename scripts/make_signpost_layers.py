"""Draw the text layers of the Chinese signposts (developer tool; needs Pillow).

Writes translation/textures/signposts/<entry>.png: 64x32 RGBA, transparent except the name (light fill, dark
outline). The layers are this project's own drawing; no game art goes in. The front page lays each one over the
player's blank board (bitmap16.flx entry 3381) to make the board (docs/formats/U9-BITMAP16-NOTES.md).

    python scripts/make_signpost_layers.py --font <NotoSansTC-VF.ttf>

Pillow is used only here, on the developer's machine; the layers are committed, so building a release does not
need it. The font must allow this use (Noto Sans TC: SIL Open Font License 1.1).
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "translation" / "textures" / "signposts"
SIGNS = {   # bitmap16.flx entry -> approved name (translation/glossary/U9_TERMS.csv); 4471 Asylum stays English
    4472: "不列顛城", 4473: "小灣鎮", 4474: "昇月城", 4475: "獸爪村", 4476: "特林希克",
    4477: "瓦羅尼亞", 4478: "紫杉城", 4568: "巨大石柱", 4569: "石門",
}
WIDTH, HEIGHT, SIZE, SCALE, RAISE = 64, 32, 12, 4, 1
FILL, OUTLINE = (236, 232, 220, 255), (18, 10, 8, 255)


def layer(text, font_path):
    from PIL import Image, ImageDraw, ImageFont
    w, h = WIDTH * SCALE, HEIGHT * SCALE
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    font = ImageFont.truetype(str(font_path), SIZE * SCALE)
    try:
        font.set_variation_by_name("Bold")
    except (OSError, AttributeError):
        pass   # not a variable font
    box = draw.textbbox((0, 0), text, font=font, stroke_width=SCALE)
    x = (w - (box[2] - box[0])) // 2 - box[0]
    y = (h - (box[3] - box[1])) // 2 - box[1] - RAISE * SCALE
    draw.text((x, y), text, font=font, fill=FILL, stroke_width=SCALE, stroke_fill=OUTLINE)
    return img.resize((WIDTH, HEIGHT), Image.LANCZOS)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--font", type=Path, required=True)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    for entry, text in SIGNS.items():
        layer(text, args.font).save(OUT / f"{entry}.png", optimize=True)
    meta = {"base_entry": 3381, "width": WIDTH, "height": HEIGHT,
            "font": args.font.name, "size_px": SIZE, "weight": "Bold",
            "signs": {str(k): v for k, v in SIGNS.items()}}
    (OUT / "signposts.json").write_text(json.dumps(meta, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(f"{len(SIGNS)} layers in {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
