# U9 在 Wine（Mac CrossOver 的預先測試）

目的：在 Mac 實機測試之前，先在 Windows 上的 WSL2＋Wine 找出相容問題。
CrossOver 的核心是 Wine，所以這裡遇到的問題，Mac 上多半也會遇到。
不過 Mac 實機仍要另外驗證：Apple Silicon 上還多了 Rosetta 2，而且 CrossOver 的圖形層也可能不同。

## 測試環境（2026-11-23）

- WSL2 Ubuntu 26.04 LTS，x86_64，WSLg（有畫面、有聲音）。
- Wine 10.0（Ubuntu 套件 `10.0~repack-12ubuntu1`），64 位元環境，模擬 Windows 10。
- U9 的 32 位元程式以 **wow64 模式**執行（記錄：`starting ... u9.exe in experimental wow64 mode`）。
  Mac 上的 CrossOver 也用 wow64 跑 32 位元程式，所以這一點和 Mac 相同。
- 遊戲：Steam 私人副本的複本，`u9.exe` SHA-256 `339a5ba8…0e152`。
  複本只改兩處：`Options.ini` 的解析度改成 1024×768；`dxcfg.ini` 的 `display=1024x768@60`。
- 遊戲以 `wine explorer /desktop=u9,1024x768 u9.exe` 在虛擬桌面中執行。
- Wine 的記錄等級：`WINEDEBUG=err+all,fixme-all,+loaddll`。

## 原版結果

| 輪 | DirectDraw | 結果 |
| --- | --- | --- |
| w01 | Wine 內建（`ddraw=b`，wined3d → OpenGL） | 3D 場景正常；**介面圖片只剩黑色剪影，書頁沒有字** |
| w02 | 遊戲附的 GOG 包裝（`ddraw=n`） | 介面、書頁正常 |
| w03 | GOG 包裝，`dxcfg.ini` 改成 1024×768 | 介面、書頁正常；影片正常（月之門後的動畫） |
| w06 | GOG 包裝，1280×960 | 切換視窗幾次後收不到滑鼠（見下方問題 3） |

- 遊戲附的 `ddraw.dll` 是「GOG.com DirectX 1-7 wrapper」1.1.0.6423（SHA-256 `f045795c…a502`）。
  它把 DirectX 7 轉成 Direct3D 9。在 Wine 下的路徑是：GOG 包裝 → Wine d3d9 → wined3d → OpenGL。
- **Wine 預設優先使用自己內建的 DLL**，不會用遊戲資料夾裡的 `ddraw.dll`。
  要用 GOG 包裝，必須設定 DLL 覆寫 `ddraw=n`（CONFIRMED：w01 記錄是 `DDRAW.dll ... builtin`，w02 是 `native`）。
  在 Mac 上的 CrossOver，也要對這個環境設定同樣的覆寫。
- w01 介面變黑的原因沒有追查（HYPOTHESIS：Wine 內建的 DirectDraw 沒有更新執行中才寫入的貼圖）。改用 GOG 包裝就沒有這個問題。
- 影片：三輪都沒有載入 Wine 的 GStreamer 元件，只載入了 `Quartz.dll`（內建）和遊戲附的 `dshow.dll`（native）。
  影片可以正常播放，所以應該是 GOG 的 `dshow.dll` 自己解碼（STRONG EVIDENCE）。
  CodeWeavers 舊回報（CrossOver 12）說影片不能播，和這次的結果不同。
- 介面點擊：要先按 Q 切到滑鼠模式，和原版在 Windows 上的操作相同，不是 Wine 的問題。
- 除了 wow64 那一行提示，三輪的 Wine 記錄都沒有錯誤，結束代碼都是 0。

## 中文化結果（內部測試包 `U9CHT-0.1.0-internal`，經首頁啟動）

設定：DLL 覆寫 `ddraw=n;dsound=n,b`；首頁和遊戲在 1280×1024 的虛擬桌面中執行。

| 輪 | 結果 |
| --- | --- |
| w04 | 首頁中文全是方框；開始遊戲時，runtime 初始化失敗（錯誤 9），遊戲停止 |
| w05 | 修正兩處後：首頁中文正常；進得了遊戲，對話、介面、影片字幕都是中文 |

w04 也證實了下列幾項：
- 首頁在 Wine 下會從玩家的原檔產生 `data/static/`。
- 加上 `dsound=n,b` 後，Wine 會載入代理 `dsound.dll`。
- 結束後，代理會被移除，四個 FLX 也會還原（與原版比對相同）。

### 問題 1：首頁中文是方框（CONFIRMED，已修）

- 首頁原本固定用「Microsoft JhengHei UI」。Wine 和 CrossOver 都沒有這個字型。
- 修正：找不到這個字型時，首頁以私有方式（`FR_PRIVATE`）載入測試包附的字型，字型族名從字型檔的 name 表讀出。
- Windows 上照舊用正黑體。測試用參數 `--ui-font bundled` 可以在 Windows 上強制改用附帶的字型。
- 未處理：視窗標題列和訊息框用的是系統字型，在 Wine 下可能仍是方框。

### 問題 2：runtime 初始化失敗，錯誤 9（CONFIRMED，已修）

- 原因：字形快取的檢查 `non-integral engine logical metric`。
  - Wine 10 量 jf-openhuninn 的 U+2015「―」：字高 26 時寬 **25**，字高 32 時寬 **33**。
  - Windows GDI 在相同條件下量出 26、32。用 ctypes 呼叫 `GetGlyphOutlineW`，在字高 20、26、32、40 都量過。
  - 字型檔 hmtx 裡，這個字的設計寬度是 1024/1024，正好一個字寬。
- 修正：排版格字形的寬度只差 1 像素，而且字型的設計寬度換算後正好是整數像素時，改用設計寬度。差更多仍照樣停止。
  - 函式是 `snap_cell_advance`、`font_advances_from_sfnt`，有單元測試。
  - 記錄欄位 `texture_glyphs.advance_snaps`：Wine 在 1280×960 下是 **3**。Windows 上兩邊本來就一致，應為 0。
- 附帶的改進：`runtime_fail` 現在多了 `reason`（失敗的檢查是哪一項）。字寬錯誤的訊息也會寫出字碼、字高和寬度。原本只有錯誤編號，查不出原因。

### 問題 3：切換視窗後，遊戲收不到滑鼠（Wine 的問題，未處理）

- 現象：切到別的視窗再切回來，幾次之後遊戲收不到滑鼠，點不到 UI，選項書也開不了。
- **原版也會**（w06，未放中文化）。所以不是中文化造成的（STRONG EVIDENCE）。
- 這台電腦的 Wine 在 WSLg 下用 X11 驅動。Mac 的 CrossOver 用 macdrv，焦點處理方式不同，所以在 Mac 上不一定會發生。要請 Mac 測試者特別看。
- 可以試的繞過方法（未測）：Wine 的 DirectInput 設定 `MouseWarpOverride`；或不用虛擬桌面。

## 給 Mac（CrossOver）的初步設定

1. 用 GOG 的離線安裝檔，在 CrossOver 的環境（bottle）裡安裝遊戲。
2. 對這個環境設定 DLL 覆寫：
   - `ddraw` 設成「native」：用遊戲附的 GOG 包裝；
   - `dsound` 設成「native, builtin」：載入中文化的代理。
3. 解壓中文化，在 CrossOver 裡執行 `U9Cht.exe`。

以上都還沒在 Mac 上驗證。
