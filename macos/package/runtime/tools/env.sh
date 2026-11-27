# 共用環境變數（由安裝／啟動腳本 source）
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RT="$ROOT/runtime"
# Wine 引擎：Sikarugir Wine 10.0（rev 8），由安裝程式下載。
# 官方 Wine 10/11 在 Apple Silicon 上執行 32 位元程式會隨機當機（WineHQ bug 60494），所以不用。
WINEBIN="$RT/wine/sk10/wswine.bundle/bin"
export DYLD_FALLBACK_LIBRARY_PATH="$RT/wine/sk10/libs:/usr/lib"
export WINEPREFIX="$ROOT/prefix"
export WINEDEBUG="${WINEDEBUG:--all}"
# ddraw：使用 GOG 附的包裝器；dsound：中文化的 dsound.dll 代理
export WINEDLLOVERRIDES="ddraw,dsound=n,b;mscoree,mshtml="
export PATH="$WINEBIN:$PATH"
GAMEDIR="$WINEPREFIX/drive_c/GOG Games/Ultima 9"
GAMEDIR_WIN='C:\GOG Games\Ultima 9'
CHTDIR="$WINEPREFIX/drive_c/U9CHT"
DISP="$RT/tools/u9display"
