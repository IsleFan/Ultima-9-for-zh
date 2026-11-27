#!/bin/bash
# 創世紀 IX 繁體中文版 macOS 安裝程式（U9CHT-@VERSION@-macOS）
# 用法：雙擊執行（以對話框操作）
#   進階：1_安裝.command --source <GOG 安裝檔 .exe 或 遊戲資料夾> [--yes]
source "$(dirname "$0")/runtime/tools/env.sh"
cd "$ROOT" || exit 1

SOURCE=""; YES=0
while [ $# -gt 0 ]; do
    case "$1" in
        --source) SOURCE="$2"; shift 2 ;;
        --yes) YES=1; shift ;;
        *) shift ;;
    esac
done

say()  { echo "▶ $*"; }
fail() {
    echo "✖ $*"
    [ "$YES" = 1 ] || osascript -e "display alert \"安裝失敗\" message \"$*\" as critical" >/dev/null 2>&1
    exit 1
}
ask() {  # ask "訊息" "按鈕1" "按鈕2" ... → 印出按下的按鈕
    local msg="$1"; shift
    local btns; btns=$(printf '"%s",' "$@"); btns="${btns%,}"
    osascript -e "button returned of (display dialog \"$msg\" with title \"U9 中文版安裝\" buttons {$btns} default button 1)" 2>/dev/null
}

echo "==== 創世紀 IX 繁體中文版 macOS 安裝 ===="

# 1. 系統檢查
OSV=$(sw_vers -productVersion)
OS_MAJ=${OSV%%.*}
# Sikarugir Wine 10.0 的執行檔要求 macOS 14 以上（LC_BUILD_VERSION minos 14.0）
if [ "$OS_MAJ" -lt 14 ]; then
    fail "需要 macOS 14 Sonoma 以上（目前 $OSV）"
fi
say "macOS $OSV，$(uname -m)"
if [ "$(uname -m)" = "arm64" ] && ! arch -x86_64 /usr/bin/true 2>/dev/null; then
    say "Apple Silicon 需要 Rosetta 2"
    if [ "$YES" = 1 ] || [ "$(ask "這台 Mac 尚未安裝 Rosetta 2（執行 Wine 必需）。要現在安裝嗎？" "安裝" "取消")" = "安裝" ]; then
        softwareupdate --install-rosetta --agree-to-license || fail "Rosetta 2 安裝失敗"
    else
        exit 0
    fi
fi

# 2. 解除隔離屬性（從網路下載的檔案）
xattr -dr com.apple.quarantine "$ROOT" 2>/dev/null

# 3. 既有安裝
if [ -f "$GAMEDIR/u9.exe" ]; then
    if [ "$YES" != 1 ] && [ "$(ask "已經安裝過了。要重新安裝嗎？（存檔會保留）" "重新安裝" "取消")" != "重新安裝" ]; then
        exit 0
    fi
fi

# 4. 選擇遊戲來源
if [ -z "$SOURCE" ]; then
    KIND=$(ask "請選擇你的 Ultima IX（GOG 或 Steam 版 1.19f）來源：

• GOG 安裝檔：setup_ultima_ix_…exe（同一資料夾要有 .bin 檔）
• 遊戲資料夾：已安裝好、裡面有 u9.exe 的資料夾" "GOG 安裝檔" "遊戲資料夾" "取消")
    case "$KIND" in
        "GOG 安裝檔") SOURCE=$(osascript -e 'POSIX path of (choose file with prompt "選擇 GOG 安裝檔 setup_ultima_ix_….exe")' 2>/dev/null) ;;
        "遊戲資料夾") SOURCE=$(osascript -e 'POSIX path of (choose folder with prompt "選擇有 u9.exe 的遊戲資料夾")' 2>/dev/null) ;;
        *) exit 0 ;;
    esac
    [ -z "$SOURCE" ] && exit 0
fi
SOURCE="${SOURCE%/}"
if [ -d "$SOURCE" ]; then
    [ -f "$SOURCE/u9.exe" ] || fail "這個資料夾裡沒有 u9.exe：$SOURCE"
    MODE=folder
elif [ -f "$SOURCE" ] && [[ "$SOURCE" == *.exe ]]; then
    MODE=installer
else
    fail "無法辨識的來源：$SOURCE"
fi
say "遊戲來源：$SOURCE"

# 5. Wine 引擎（不隨安裝包附帶：已有就直接用，找到已下載的檔案就用它，否則自動下載）
ENGINE_DIR="$RT/wine/sk10"
ENGINE_FILE="WS12WineSikarugir10.0_8.tar.xz"
ENGINE_URL="https://github.com/Sikarugir-App/Engines/releases/download/v1.0/$ENGINE_FILE"
ENGINE_SHA="2a6bcf2a3bf13cddf825599b6bc1e3659cbf71e1282c6d77141a92f6ac8d4c68"
engine_ok() { [ "$(shasum -a 256 "$1" 2>/dev/null | cut -d' ' -f1)" = "$ENGINE_SHA" ]; }
if [ -x "$ENGINE_DIR/wswine.bundle/bin/wine" ]; then
    say "Wine 引擎已安裝"
else
    TARBALL=""
    for d in "$ROOT" "$RT" "$HOME/Downloads"; do
        if [ -f "$d/$ENGINE_FILE" ]; then
            if engine_ok "$d/$ENGINE_FILE"; then
                TARBALL="$d/$ENGINE_FILE"; say "找到已下載的 Wine 引擎：$TARBALL"; break
            else
                say "略過 $d/$ENGINE_FILE（檔案不完整或檢查碼不符）"
            fi
        fi
    done
    if [ -z "$TARBALL" ]; then
        say "下載 Wine 引擎（約 166 MB，需要網路連線）…"
        TARBALL="$ROOT/$ENGINE_FILE"
        rm -f "$TARBALL.part"
        if ! curl -L --fail --progress-bar -o "$TARBALL.part" "$ENGINE_URL"; then
            rm -f "$TARBALL.part"
            fail "下載 Wine 引擎失敗。請確認網路連線後重試；或用瀏覽器下載 $ENGINE_URL ，把檔案放進本資料夾（或「下載項目」）後重新執行安裝。"
        fi
        if ! engine_ok "$TARBALL.part"; then
            rm -f "$TARBALL.part"
            fail "下載的 Wine 引擎檢查碼不符，已刪除。請重新執行安裝。"
        fi
        mv "$TARBALL.part" "$TARBALL"
        DOWNLOADED=1
    fi
    say "解開 Wine 引擎…"
    rm -rf "$ENGINE_DIR/wswine.bundle"
    tar -xJf "$TARBALL" -C "$ENGINE_DIR" || fail "解開 Wine 引擎失敗"
    xattr -dr com.apple.quarantine "$ENGINE_DIR" 2>/dev/null
    [ -x "$ENGINE_DIR/wswine.bundle/bin/wine" ] || fail "Wine 引擎內容不正確"
    # 自動下載的檔案用完即刪；使用者自己放的檔案保留
    [ "${DOWNLOADED:-0}" = 1 ] && rm -f "$TARBALL"
fi

# 6. 建立 Wine 環境
wineserver -k 2>/dev/null
if [ ! -f "$WINEPREFIX/system.reg" ]; then
    say "建立 Wine 環境（第一次約需 1 分鐘）…"
    WINEDLLOVERRIDES="mscoree,mshtml=" wine wineboot -i >/dev/null 2>&1
    wineserver -w
    [ -f "$WINEPREFIX/system.reg" ] || fail "Wine 環境建立失敗"
fi

# 7. 放入遊戲檔案（保留既有存檔）
SAVE_BAK=""
if [ -d "$GAMEDIR/savegame" ] && [ -n "$(ls -A "$GAMEDIR/savegame" 2>/dev/null)" ]; then
    SAVE_BAK="$ROOT/savegame-backup-$(date +%Y%m%d-%H%M%S)"
    say "備份既有存檔到 $(basename "$SAVE_BAK")"
    ditto "$GAMEDIR/savegame" "$SAVE_BAK"
fi
rm -rf "$GAMEDIR"
mkdir -p "$GAMEDIR"
if [ "$MODE" = folder ]; then
    say "複製遊戲檔案（約 1.6 GB）…"
    ditto "$SOURCE" "$GAMEDIR" || fail "複製遊戲檔案失敗"
    find "$GAMEDIR" -name .DS_Store -delete
else
    say "解開 GOG 安裝檔（約 1.6 GB）…"
    ( cd "$(dirname "$SOURCE")" && wine "$RT/tools/innoextract.exe" -e -q -d "$GAMEDIR_WIN" "$(basename "$SOURCE")" ) 2>&1 | grep -vE "^\s|mvk"
    wineserver -w
    [ -f "$GAMEDIR/u9.exe" ] || fail "解開安裝檔失敗。請確認 .bin 檔和 .exe 放在同一個資料夾。"
    if [ -d "$GAMEDIR/app" ]; then ditto "$GAMEDIR/app" "$GAMEDIR"; fi
    rm -rf "$GAMEDIR/app" "$GAMEDIR/tmp" "$GAMEDIR/__redist" "$GAMEDIR/commonappdata"
fi
mkdir -p "$GAMEDIR/savegame"
if [ -n "$SAVE_BAK" ]; then
    ditto "$SAVE_BAK" "$GAMEDIR/savegame"
    say "已還原存檔"
fi
[ -d "$GAMEDIR/static" ] || fail "遊戲檔案不完整（缺少 static 資料夾）"

# 8. 中文化
say "安裝中文化…"
rm -rf "$CHTDIR"
ditto "$RT/U9CHT" "$CHTDIR"

# 9. 登錄設定
wine reg add 'HKCU\Software\Origin Systems\Ultima IX' /v 'Install Path' /t REG_SZ /d "$GAMEDIR_WIN" /f >/dev/null 2>&1
wine reg add 'HKCU\Software\Wine\DllOverrides' /v ddraw /t REG_SZ /d native,builtin /f >/dev/null 2>&1
wine reg add 'HKCU\Software\Wine\DllOverrides' /v dsound /t REG_SZ /d native,builtin /f >/dev/null 2>&1
wineserver -w

say "安裝完成"
[ "$YES" = 1 ] || osascript -e 'display dialog "安裝完成！

請雙擊「2_啟動 U9 中文版.command」開始遊戲。" with title "U9 中文版安裝" buttons {"好"} default button 1' >/dev/null 2>&1
exit 0
