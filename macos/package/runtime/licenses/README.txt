本安裝包使用的第三方元件與授權
================================

本安裝包「不含」任何 Ultima IX 遊戲檔案。遊戲需由使用者自備正版（GOG 或 Steam 版 1.19f）。
Ultima 為 Electronic Arts 的商標。本中文化與 Mac 執行環境為玩家自製，與 Electronic Arts、
Origin Systems、GOG、Valve、Apple 無關。

1. Wine 10.0（Sikarugir 建置，rev 8）— runtime/wine/sk10/wswine.bundle
   ※ 本安裝包「不附帶」這個引擎。安裝程式會在安裝時從下列建置來源下載
     WS12WineSikarugir10.0_8.tar.xz，並以 SHA-256 驗證後解開到上述位置。
   授權：GNU LGPL 2.1 以上  https://www.winehq.org/license
   建置來源：https://github.com/Sikarugir-App/Engines （v1.0，未修改）
   原始碼：https://github.com/Sikarugir-App （Wine 原始碼：https://gitlab.winehq.org/wine/wine ）

   【免費說明】Wine 是 LGPL 開放原始碼軟體；Sikarugir 是社群開源專案，其建置可免費使用與散布，
   不需購買 CrossOver 或其他商業軟體。它不含 Apple 的 D3DMetal／Game Porting Toolkit，
   與 CodeWeavers（CrossOver）、WineHQ 官方無關，也不受其支援。

   runtime/wine/sk10/libs：上述引擎執行所需的函式庫（FreeType、MoltenVK、GStreamer、GLib、
   GnuTLS、SDL2 等），取自 WineHQ 官方 macOS 套件 11.21
   （https://github.com/Gcenx/macOS_Wine_builds），未修改；各自依其原專案授權
   （FTL/GPLv2、Apache 2.0、LGPL、zlib 等）。

2. innoextract 1.9 — runtime/tools/innoextract.exe
   授權：zlib；內含 Boost、liblzma、libstdc++ 執行期，授權全文見 licenses/innoextract/
   來源：https://github.com/dscharrer/innoextract/releases/tag/1.9 （官方 Windows 版，未修改）

3. 創世紀 IX 繁體中文版 @VERSION@ — runtime/U9CHT
   原作者：https://github.com/pmanyeh/Ultima-9-for-zh （發佈 v@VERSION@，未修改）
   授權：MIT（runtime/U9CHT/LICENSE.txt）；其餘第三方授權見 runtime/U9CHT/THIRD_PARTY_NOTICES.md
   字型 jf open 粉圓（SIL OFL 1.1）授權見 runtime/U9CHT/fonts/jf-openhuninn-LICENSE.txt

4. Mac 輔助工具（本安裝包自製）
   - u9display：列出螢幕、暫時切換主要顯示器、還原（原始碼在專案的 macos/src/u9display.swift）
