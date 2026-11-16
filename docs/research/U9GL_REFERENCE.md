# U9GL（tgwUltima9GL）參考評估

- 專案：<https://github.com/theGreyWanderer-uc/tgwUltima9GL>
- 檢視的 commit：`8c3affc91db2c66ecb197f68510ccd2814dbcb25`（U9GL 0.5.8，2026-11-16）
- 檢視方式：只讀原始碼，沒有建置、沒有執行、沒有裝進任何遊戲副本。本機副本放在 `.local/upstream/tgwUltima9GL`（不提交）。
- 作者與 TITAN／tgwUltima 相同，但授權不同（見下）。

## 它是什麼

替 Ultima IX 加上第四個渲染器（OpenGL）、一個 .NET 啟動器，以及一批原版錯誤修正與畫面強化。
沒有任何文字、字型、FLX、語言相關的程式（CONFIRMED，全文搜尋）。

## 授權

「U9GL Non-Commercial Copyleft Licence 1.0」，自訂的 source-available 授權，不是 MIT。

- 個人非商業使用可以讀、改、散布，但散布衍生作品必須用同一份授權並附原始碼（§4、§5）。
- 因此**不可把它的程式碼複製進本專案**。TITAN 的 MIT 授權不適用於這個專案。
- 位址與行為這類事實可以當作線索，但要自己在 `u9.exe` 上驗證後再寫進我們的文件，並註明出處。

## 與我們 runtime 的衝突

| 項目 | 等級 | 內容 |
| --- | --- | --- |
| 載入器同名 | CONFIRMED | U9GL 在 `u9.exe` 旁放 `dsound.dll`（`u9gl/src/loader/dsound_proxy.c`），再載入 `u9gl\u9log.dll`。我們的代理也叫 `dsound.dll`。我們的啟動器遇到別人的 `dsound.dll` 會拒絕啟動，所以裝了 U9GL 的玩家目前無法用我們的中文化。 |
| 渲染器 | CONFIRMED（程式碼）／未實測 | 我們的中文繪字綁定 DX7 渲染器：`engine.cpp` 三處檢查 vtable `0x74FBB8`，並掛它的 `+0x60` 項。U9GL 要求 `Options.ini` 用 rasterizer 3（Glide），再換成自己的渲染器（`diag/factory.c`「rasterizer 3 -> 5」）。兩者的前提互斥。 |
| 訊息迴圈 | STRONG EVIDENCE | 兩邊都改 IAT 的 `PeekMessageA`（`0x740374`）與 `GetMessageA`（`0x740378`）。U9GL 用在 `fix/clockfix.c`。能否串接取決於載入順序與對方的位元組檢查，未驗證。 |
| 其他掛鉤點 | STRONG EVIDENCE | 比對我們 110 個、U9GL 665 個寫在原始碼裡的位址，除上列外沒有 16 bytes 內的重疊。只比對了字面位址，計算出來的位址不在內。 |

## 可以參考的地方

- `u9gl/src/common/symbols.c`：306 個附說明的位址；`u9gl/addresses.h`：渲染器物件的欄位。和我們有交集的例子：
  - `0x624680`：在世界物件上顯示一行文字（我們的 `clock_text.h` 已用到同一函式）。
  - `0x585390` PlayMovie 與它的呼叫點、`0x584E40` 停止影片。
  - `0x73FA7F`：遊戲失去焦點時自己呼叫 `ShowWindow(SW_SHOWMINIMIZED)`。這可以解釋 session 24「視窗被最小化」（HYPOTHESIS，待對照）。
  - 鏡頭投影欄位（`U9_CAM_PROJ_SX` 等）：當初因為「需要鏡頭投影」而不做的頭頂血條，這裡有線索。
- 原版錯誤修正清單（`u9gl/u9gl.ini` 的 Fixes 區段）。與我們待辦可能相關、但**尚未確認是同一個錯誤**的：
  - `fix_absent_target`（聖者倒下後怪物詢問不存在的目標而閃退）對照我們的「鎖定敵人時死亡再讀檔閃退」。
  - `fix_f3`（讀檔閃退）。
  - `fix_clock_rebase`（Windows 開機約 25 天後動畫變慢）對照效能待辦。
- 做法上的參考：每個修補都先比對預期位元組、有修補清單可輸出、每個修正有獨立開關。

## 效能

- U9GL 沒有宣稱比原本的渲染器快；更新記錄提到效能的地方都是新效果的代價。
- 使用者實測（2026-11-16，U9GL 單獨使用，設定與測試場景未記錄）：沒有比較順，反而更延遲。只有一次主觀觀察，沒有數據。
- 因此效能問題不靠換渲染器解決，仍照 `docs/HANDOFF.md` 第 6 節的比較計畫。

## 結論

- 對中文化本身（文字、字型、排版）沒有直接幫助。
- 是目前最完整的 `u9.exe` 行為與位址參考，適合查原版錯誤與引擎結構。
- 兩個專案目前不能同時使用。要共存需要：載入器串接，以及讓中文繪字支援 DX7 以外的渲染器。後者屬於新的 renderer 工作，依 AGENTS §14 需要明確授權。
