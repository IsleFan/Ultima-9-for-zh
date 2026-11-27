#!/bin/bash
# 打包 macOS 安裝包 U9CHT-<版本>-macOS.zip
#
# 用法：macos/build_package.sh --cht <U9CHT-x.y.z 資料夾或 zip> [--version x.y.z] [--out <資料夾>]
#   --cht      原作者發佈的中文化（資料夾或 zip，例如 U9CHT-1.0.0.zip），原樣放進 runtime/U9CHT
#   --version  省略時從 --cht 的名稱取（U9CHT-1.0.0 → 1.0.0）
#   --out      輸出資料夾，預設 .local/macos-build（已在 .gitignore）
#
# 需要：macOS、Xcode Command Line Tools（swiftc、lipo）、curl、ditto。
# 第三方檔案一律從官方來源下載並比對 SHA-256，快取在 <out>/cache；Wine 引擎本身不打包，
# 由安裝程式在使用者電腦上下載（見 package/1_安裝.command）。
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
CHT=""; VERSION=""; OUT="$HERE/../.local/macos-build"
while [ $# -gt 0 ]; do
    case "$1" in
        --cht) CHT="$2"; shift 2 ;;
        --version) VERSION="$2"; shift 2 ;;
        --out) OUT="$2"; shift 2 ;;
        *) echo "未知參數：$1" >&2; exit 2 ;;
    esac
done
[ -n "$CHT" ] || { echo "需要 --cht <U9CHT-x.y.z 資料夾或 zip>" >&2; exit 2; }
[ -e "$CHT" ] || { echo "找不到 $CHT" >&2; exit 2; }
if [ -z "$VERSION" ]; then
    VERSION=$(basename "$CHT" | sed -nE 's/^U9CHT-([0-9]+\.[0-9]+\.[0-9]+[A-Za-z0-9.-]*)(\.zip)?$/\1/p')
    [ -n "$VERSION" ] || { echo "無法從名稱判斷版本，請加 --version" >&2; exit 2; }
fi
mkdir -p "$OUT/cache"
OUT="$(cd "$OUT" && pwd)"
NAME="U9CHT-$VERSION-macOS"
STAGE="$OUT/$NAME"
echo "▶ 打包 $NAME"

# 第三方檔案：名稱、網址、SHA-256
INNO_FILE=innoextract-1.9-windows.zip
INNO_URL=https://github.com/dscharrer/innoextract/releases/download/1.9/$INNO_FILE
INNO_SHA=6989342c9b026a00a72a38f23b62a8e6a22cc5de69805cf47d68ac2fec993065
WINE_FILE=wine-staging-11.21-osx64.tar.xz       # 只取裡面的 dylib，給 Sikarugir 引擎用
WINE_URL=https://github.com/Gcenx/macOS_Wine_builds/releases/download/11.21/$WINE_FILE
WINE_SHA=d7bcd5abf4df27aae397a7898aef074fea4a2e0b51e0a55aa9659517bbf9d452

fetch() {  # fetch <檔名> <網址> <sha256>
    local f="$OUT/cache/$1"
    if [ ! -f "$f" ] || [ "$(shasum -a 256 "$f" | cut -d' ' -f1)" != "$3" ]; then
        echo "  下載 $1"
        curl -L --fail --silent --show-error -o "$f.part" "$2"
        mv "$f.part" "$f"
    fi
    [ "$(shasum -a 256 "$f" | cut -d' ' -f1)" = "$3" ] || { echo "$1 的 SHA-256 不符" >&2; exit 1; }
}
fetch "$INNO_FILE" "$INNO_URL" "$INNO_SHA"
fetch "$WINE_FILE" "$WINE_URL" "$WINE_SHA"

rm -rf "$STAGE" "$OUT/tmp"
mkdir -p "$STAGE/runtime/tools" "$STAGE/runtime/licenses/innoextract" "$STAGE/runtime/wine/sk10/libs" "$OUT/tmp"

# 1. 安裝包本體（範本裡的 @VERSION@ 換成版本）
for f in "1_安裝.command" "2_啟動 U9 中文版.command" "安裝與使用說明.html"; do
    sed "s/@VERSION@/$VERSION/g" "$HERE/package/$f" > "$STAGE/$f"
done
chmod +x "$STAGE/"*.command
cp "$HERE/package/runtime/tools/env.sh" "$STAGE/runtime/tools/"
sed "s/@VERSION@/$VERSION/g" "$HERE/package/runtime/licenses/README.txt" > "$STAGE/runtime/licenses/README.txt"

# 2. 中文化（原樣）
if [ -d "$CHT" ]; then
    ditto "$CHT" "$STAGE/runtime/U9CHT"
else
    ditto -x -k "$CHT" "$OUT/tmp/cht"
    SRC=$(find "$OUT/tmp/cht" -maxdepth 2 -name U9Cht.exe -print -quit)
    [ -n "$SRC" ] || { echo "zip 裡找不到 U9Cht.exe" >&2; exit 1; }
    ditto "$(dirname "$SRC")" "$STAGE/runtime/U9CHT"
fi
for f in U9Cht.exe U9ChtLauncher.exe U9ChtRuntime.dll U9ChtProxy.dll data/build-manifest.json; do
    [ -f "$STAGE/runtime/U9CHT/$f" ] || { echo "中文化缺少 $f" >&2; exit 1; }
done

# 3. innoextract（官方 Windows 版，未修改）
ditto -x -k "$OUT/cache/$INNO_FILE" "$OUT/tmp/inno"
cp "$OUT/tmp/inno/innoextract.exe" "$STAGE/runtime/tools/"
cp -R "$OUT/tmp/inno/license/"* "$OUT/tmp/inno/README.txt" "$OUT/tmp/inno/VERSION.txt" "$STAGE/runtime/licenses/innoextract/"

# 4. 引擎用的 dylib：Wine 11.21 套件 lib/ 下的 dylib，加上 GStreamer 的 GLib／GStreamer 函式庫
tar -xJf "$OUT/cache/$WINE_FILE" -C "$OUT/tmp"
L="$OUT/tmp/Wine Staging.app/Contents/Resources/wine/lib"
find "$L" -maxdepth 1 -name '*.dylib' -exec cp -a {} "$STAGE/runtime/wine/sk10/libs/" \;
for n in libffi.7.dylib libglib-2.0.0.dylib libgmodule-2.0.0.dylib libgobject-2.0.0.dylib libgstaudio-1.0.0.dylib \
         libgstbase-1.0.0.dylib libgstreamer-1.0.0.dylib libgsttag-1.0.0.dylib libgstvideo-1.0.0.dylib \
         libiconv.2.dylib libintl.8.dylib liborc-0.4.0.dylib libpcre2-8.0.dylib libz.1.dylib; do
    [ -e "$STAGE/runtime/wine/sk10/libs/$n" ] || cp -L "$L/GStreamer.framework/Versions/1.0/lib/$n" "$STAGE/runtime/wine/sk10/libs/$n"
done

# 5. u9display（universal：arm64＋x86_64）
swiftc -O -target arm64-apple-macos11 -o "$OUT/tmp/u9d-arm64" "$HERE/src/u9display.swift"
swiftc -O -target x86_64-apple-macos10.15 -o "$OUT/tmp/u9d-x86_64" "$HERE/src/u9display.swift"
lipo -create -output "$STAGE/runtime/tools/u9display" "$OUT/tmp/u9d-arm64" "$OUT/tmp/u9d-x86_64"

# 6. 壓縮
find "$STAGE" -name .DS_Store -delete
rm -rf "$OUT/tmp"
rm -f "$OUT/$NAME.zip" "$OUT/$NAME.zip.sha256"
( cd "$OUT" && ditto -c -k --sequesterRsrc --keepParent "$NAME" "$NAME.zip" && shasum -a 256 "$NAME.zip" > "$NAME.zip.sha256" )
echo "▶ 完成：$OUT/$NAME.zip"
cat "$OUT/$NAME.zip.sha256"
