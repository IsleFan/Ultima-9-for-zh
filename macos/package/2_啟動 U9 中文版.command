#!/bin/bash
# 雙擊啟動 創世紀 IX 繁體中文版（U9Cht.exe 中文化首頁）
# - 多個螢幕時先選螢幕：暫時設為主要顯示器，離開遊戲（u9.exe 結束）後立即還原
# - 首頁視窗依螢幕高度自動縮小（調低 Wine DPI，結束後恢復 96）
source "$(dirname "$0")/runtime/tools/env.sh"
mkdir -p "$ROOT/logs"
FRONT_H=976   # 首頁在 96 DPI 時的視窗高度（點）

if [ ! -x "$WINEBIN/wine" ] || [ ! -f "$GAMEDIR/u9.exe" ] || [ ! -f "$CHTDIR/U9Cht.exe" ]; then
    osascript -e 'display alert "尚未安裝" message "請先雙擊「1_安裝.command」完成安裝。" as warning' >/dev/null 2>&1
    exit 1
fi
if pgrep -f 'U9Cht\.exe' >/dev/null; then
    osascript -e 'display alert "已在執行中" message "中文化首頁已經開著了。" as warning' >/dev/null 2>&1
    exit 0
fi

# 上次若異常結束沒還原，先還原
"$DISP" restore

LIST="$("$DISP" list)"
COUNT=$(printf '%s\n' "$LIST" | grep -c .)
N=$(printf '%s\n' "$LIST" | awk -F'\t' '$4=="main"{print $1}')
if [ "$COUNT" -gt 1 ]; then
    ITEMS=$(printf '%s\n' "$LIST" | awk -F'\t' '{m=($4=="main")?"（目前主螢幕）":""; printf "%s%s. %s %s%s", (NR>1?"\",\"":""), $1, $2, $3, m}')
    DEF=$(printf '%s\n' "$LIST" | awk -F'\t' '$4=="main"{print $1". "$2" "$3"（目前主螢幕）"}')
    PICK=$(osascript -e "choose from list {\"$ITEMS\"} with title \"U9 中文版\" with prompt \"要在哪個螢幕玩？\" default items {\"$DEF\"}")
    [ "$PICK" = "false" ] && exit 0
    N="${PICK%%.*}"
    trap '"$DISP" restore' EXIT HUP INT TERM
    "$DISP" set-main "$N" || exit 1
    sleep 2
    # 離開遊戲就還原：等 u9.exe 出現，再等它結束
    (
        until pgrep -f 'u9\.exe' >/dev/null; do
            pgrep -f 'U9Cht\.exe' >/dev/null || exit 0
            sleep 1
        done
        while pgrep -f 'u9\.exe' >/dev/null; do sleep 1; done
        "$DISP" restore
    ) &
fi

# 依選定螢幕高度計算首頁 DPI（上限 96）
SCREEN_H=$(printf '%s\n' "$LIST" | awk -F'\t' -v n="$N" '$1==n{split($3,a,"x"); print a[2]}')
DPI=$(( 96 * (${SCREEN_H:-1080} - 35) / FRONT_H ))
[ "$DPI" -gt 96 ] && DPI=96

echo "遊戲執行中，請勿關閉此視窗（離開遊戲後會自動還原螢幕設定）"
# Wine 10：視窗大小讀 Control Panel\Desktop，內容縮放讀 Software\Wine\Fonts，兩個要一致
dpi_set() {
    wine reg add 'HKCU\Control Panel\Desktop' /v LogPixels /t REG_DWORD /d "$1" /f >/dev/null 2>&1
    wine reg add 'HKCU\Software\Wine\Fonts' /v LogPixels /t REG_DWORD /d "$1" /f >/dev/null 2>&1
    wineserver -w
}
dpi_set "$DPI"
cd "$CHTDIR" || exit 1
wine U9Cht.exe >"$ROOT/logs/wine-last.log" 2>&1
wineserver -k
dpi_set 96
wineserver -w
echo "已結束，可以關閉此視窗"
