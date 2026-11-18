"""Build a release folder and zip (docs/release/RELEASE_PROCESS.md).

The release holds no game file and nothing that carries the game's English text:
  U9Cht.exe, U9ChtLauncher.exe, U9ChtRuntime.dll, U9ChtProxy.dll   (--runtime)
  data/build-manifest.json, data/runtime/allowlist.u9ct, data/runtime/movies.tsv   (--build)
  data/patches/<name>.u9cp   made here from the local originals (--game-dir) and the built FLX
  fonts/<font> and its license, 說明.txt, THIRD_PARTY_NOTICES.md
Every patch is applied again and must give the built file. The folder is then scanned: no FLX, no local path,
no 16+ character Latin run in any patch's added bytes. Output goes under .local/ only (it is never tracked).
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from u9cht.l10n import patch  # noqa: E402

PROGRAMS = ("U9Cht.exe", "U9ChtLauncher.exe", "U9ChtRuntime.dll", "U9ChtProxy.dll")
RESOURCES = ("text.flx", "misctext.flx", "TYPENAME.FLX", "BOOKS-EN.FLX")
DATA_FILES = ("build-manifest.json", "runtime/allowlist.u9ct", "runtime/movies.tsv")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fail(message):
    raise SystemExit(f"package_release: {message}")


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--name", required=True, help="release folder name, e.g. U9CHT-0.1.0-internal")
    ap.add_argument("--build", required=True, help="build_localization.py output folder")
    ap.add_argument("--runtime", required=True, help="folder with the four built programs")
    ap.add_argument("--game-dir", required=True, help="original game (its static/ FLX are the patch sources)")
    ap.add_argument("--font", required=True, help="font file to ship as the default")
    ap.add_argument("--output", default=str(ROOT / ".local" / "release"))
    ap.add_argument("--allow-dirty", action="store_true", help="allow tracked changes (internal test builds)")
    a = ap.parse_args(argv)

    output = Path(a.output).resolve()
    if ".local" not in output.parts:
        fail("the output must be under .local/ (a release holds built binaries and is never tracked)")
    dirty = git("status", "--porcelain", "--untracked-files=no")
    if dirty and not a.allow_dirty:
        fail("tracked files have changes; commit first or pass --allow-dirty")
    build, runtime, game = Path(a.build), Path(a.runtime), Path(a.game_dir)
    manifest = json.loads((build / "build-manifest.json").read_text(encoding="utf-8"))

    folder = output / a.name
    if folder.exists():
        shutil.rmtree(folder)
    (folder / "data" / "runtime").mkdir(parents=True)
    (folder / "data" / "patches").mkdir()
    (folder / "fonts").mkdir()

    for name in PROGRAMS:
        shutil.copy2(runtime / name, folder / name)
    for rel in DATA_FILES:
        shutil.copy2(build / rel, folder / "data" / rel)

    report = {"name": a.name, "git_commit": git("rev-parse", "HEAD"), "git_dirty": bool(dirty), "patches": {}}
    for name in RESOURCES:
        source = (game / "static" / name).read_bytes()
        target = (build / "static" / name).read_bytes()
        if sha256(source) != manifest["source_game_sha256"][name]:
            fail(f"{name}: the local original differs from the build's source")
        if sha256(target) != manifest["resource_output_sha256"][name]:
            fail(f"{name}: the built file differs from the manifest")
        p = patch.make(source, target)
        if patch.apply(source, p) != target:
            fail(f"{name}: the patch does not give the built file")
        latin = patch.latin_runs(p)
        if latin:
            fail(f"{name}: {len(latin)} Latin run(s) of 16+ characters in the added bytes")
        (folder / "data" / "patches" / f"{name}.u9cp").write_bytes(p)
        report["patches"][name] = {"source": len(source), "target": len(target), "patch": len(p), "sha256": sha256(p)}

    font = Path(a.font)
    shutil.copy2(font, folder / "fonts" / font.name)
    shutil.copy2(ROOT / "release" / "jf-openhuninn-LICENSE.txt", folder / "fonts" / "jf-openhuninn-LICENSE.txt")
    shutil.copy2(ROOT / "release" / "說明.txt", folder / "說明.txt")
    shutil.copy2(ROOT / "THIRD_PARTY_NOTICES.md", folder / "THIRD_PARTY_NOTICES.md")

    # Scans of the finished folder.
    local_path = re.compile(rb"[A-Za-z]:\\\\?(?:Users|git|Program Files)|/Users/|\.local[\\/]", re.I)
    files = sorted(p for p in folder.rglob("*") if p.is_file())
    for f in files:
        if f.suffix.lower() == ".flx":
            fail(f"FLX in the release: {f.relative_to(folder)}")
        if f.suffix.lower() in (".json", ".tsv", ".txt", ".md", ".u9ct", ".u9cp"):
            data = f.read_bytes()
            for text in (data, data.decode("utf-16-le", "ignore").encode("utf-8", "ignore")):
                if local_path.search(text):
                    fail(f"local path in {f.relative_to(folder)}")
    report["files"] = {str(f.relative_to(folder)).replace("\\", "/"): {"size": f.stat().st_size,
                                                                      "sha256": sha256(f.read_bytes())} for f in files}

    zip_path = output / f"{a.name}.zip"
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for f in files:
            z.write(f, f"{a.name}/{f.relative_to(folder).as_posix()}")
    report["zip"] = {"file": zip_path.name, "size": zip_path.stat().st_size, "sha256": sha256(zip_path.read_bytes())}
    (output / f"{a.name}.report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"zip": str(zip_path), "size": report["zip"]["size"], "files": len(files),
                      "patches": {k: v["patch"] for k, v in report["patches"].items()}}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
