# U9-CHT Handoff（2026-11-15）

給下一個工作階段（人或 AI Agent）的交接。
開工前仍須依 `AGENTS.md` 讀取 `AGENTS.md` 與 `ULTIMA_IX_CHT_MASTER_PLAN.md`。

**最近完成（2026-11-12～15）：**
- 作弊鍵與施法提示（開發用）、法力條法術名寬度修正。
- 體力／法力數值、傷害／法力飄字、敵人血條，三者都有首頁開關。
- 「鎖定敵人時死亡再讀檔閃退」這個原版錯誤的繞過方法。

- 遊戲中即時套用按鍵（2026-11-15，使用者授權）：首頁在遊戲進行中也能改鍵，存檔後 runtime 用遊戲自己的函式重讀按鍵檔。session 60 遊戲內驗證通過（CONFIRMED）。細節見研究文件 "Changing bindings while the game runs"。
- 啟動器在遊戲結束時就先還原遊戲檔（`--restore-backup`），首頁被防毒移除也不會留下中文檔。
- 首頁新增「按鍵設定」頁（2026-11-15；session 57、58 遊戲內驗證通過）：
  - 研究與遊戲讀按鍵檔的規則：`docs/research/U9_EXTENDED_SETUP_TOOL.md`；用法：`runtime/U9ChtFront/README.md`。
  - 程式：`runtime/U9ChtFront/keymap.h`、`keymap.cpp`；測試 `U9ChtKeymapTests`、`U9ChtKeymapLiveTests`（C++ 測試現在是 13 個）。
  - 存檔寫到遊戲資料夾的 `U9CHT.kmp`，並改 `Options.ini` 的 `Current_Keymap`；不動 `Default.kmp`。
  - 靜態分析發現 `abort_targeting` 不在 `u9.exe` 裡：原版按鍵檔的 A 鍵很可能沒有作用，待進遊戲確認。

細節見第 3 節。

---

## 0. 新對話開工清單

1. 讀 `AGENTS.md`、`ULTIMA_IX_CHT_MASTER_PLAN.md`，以及本文件第 1、2 節。
2. 建置 runtime（從 repo 根目錄執行）：
   ```sh
   "/c/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build .local/p10-misc/build --config Release
   ```
   - 新增測試檔後，要先執行 `cmake -S runtime -B .local/p10-misc/build`。
   - C++ 測試：在 `.local/p10-misc/build` 執行 `ctest -C Release`。
3. Python 測試：`python -m unittest discover -s tests`。
4. 進遊戲測試用 Steam 私人副本，使用者說「準備好了」之後才執行：
   ```sh
   python scripts/run_p8_0_session.py start --session .local/p10-misc/session-NN --label "..." \
     --game-dir .local/steam-game --build .local/p10-misc/out-m26 --runtime .local/p10-misc/build/Release \
     --config .local/p9-2/u9cht.ini --preserve-saves --seed-saves .local/p10-misc/session-29/final-savegame \
     --keymap .local/p10-misc/keymap/U9CHT-TEST.kmp
   python scripts/run_p8_0_session.py finish --session .local/p10-misc/session-NN
   ```
   - 下一個 session 編號從 **61** 開始。
   - 不加 `--keymap` 就用原本的按鍵檔：沒有作弊鍵、作弊提示和施法攔截。
   - 要關掉個別功能，就用另一個設定檔，加上 `[features]` 區段。範例：`.local/p10-misc/bisect/u9cht-newfeatures-off.ini`。
5. Runtime 版本號：
   - 改 runtime 名稱時，三處要一起改：`runtime/common/version_manifest.h`、`u9cht/l10n/runtime_pack.py`、`runtime/CMakeLists.txt`。
   - 改了以後要重建中文化資料，因為 pack 內嵌 runtime 名稱。
   - 目前仍是 `0.2.45`。這期間加的功能都沒有升版號。

## 1. 目前狀態一覽

- 分支：`p7-localization-pipeline`，已推送到 `origin`（2026-11-15，`63c22cb`）。
  - 本機備份分支 `backup/p7-before-gargish-rewrite` **不可推送**。這是 2026-11-11 改寫歷史前的備份。
- 工作樹：只有不可加入的未追蹤檔：`.vscode/`、`83e7052b…png`、`tools/u9xsb6bt2a*`、`tools/uixtools.zip`。
- 私人遊戲副本都已還原成原版，沒有進行中的 session：
  - Steam：`.local/steam-game`。最近的測試都用它。
  - GOG：`.local/runtime-game`。建置中文化資料時用它的原版檔。
- 測試：
  - C++：13/13（2026-11-15）。
  - Python：上次是 166 OK（3 skipped）。
- Runtime：`U9ChtRuntime/0.2.45-p11`，建置在 `.local/p10-misc/build/Release`。
- 中文化資料最新建置：`.local/p10-misc/out-m26`。
- 測試存檔：`.local/p10-misc/session-29/final-savegame`（Steam 副本）。session 30～56 都從這裡開始，沒有寫回。
- 首頁測試資料夾：`.local/p10-misc/front-release`。裡面已經是最新的 `U9Cht.exe`、launcher 和 runtime。

### 翻譯進度（全部是機器初稿 `draft`，尚未人工校稿）

| 資源 | store 檔 | 筆數 | 遊戲內確認 |
| --- | --- | --- | --- |
| 對話 text.flx | `text.jsonl` | 7,368 / 7,390 | ✅ |
| 物品名稱 TYPENAME | `typename.jsonl` | 2,026 / 2,032 | ✅ |
| 書籍 BOOKS-EN（含 56 個商店檔） | `books.jsonl` | 447 | 書本與路牌 ✅；商店清單 ❌ 未測 |
| 系統訊息 misctext.flx | `misctext.jsonl` | 242 / 340（刻意保留的英文見 3a） | 主要 UI ✅ |
| 影片字幕 | `movies.jsonl` | 42（draft 34、review 8） | 檢視工具 ✅；遊戲內 ✅（開場動畫，session 17；Esc 跳過 session 24） |
| 時鐘訊息（u9.exe 內的句型） | `runtime/U9ChtRuntime/clock_text.h` | 規則式改寫 | ✅ 遊戲內確認（2026-11-12） |

譯名權威檔 `translation/glossary/U9_TERMS.csv`：1,457 筆，其中 approved 768、todo 669、ignore 20。

## 2. 固定規則（使用者偏好）

- 一律用繁體中文回覆，包括工具呼叫之間的短訊息。
- 每次啟動遊戲前，先請使用者說「準備好了」。不送自動輸入。使用者會用遊戲內選單離開遊戲。
- 遊戲結束後一定要執行 `finish`，執行前先確認 `u9.exe` 已經結束。未經同意不可強制關閉遊戲。
- 不提交遊戲原文、字型、本機絕對路徑、英文原文。含英文原文的 workset 和語音辨識稿只放在 `.local/`。
- commit 結尾加上：`Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`。
- 新的 hook 或 renderer 工作需要使用者明確授權（AGENTS §13、§14）。
- 不可碰使用者原本的 GOG／Steam 安裝。路徑只在本機記憶裡，不寫進 repo。只用私人副本。
- 首頁測試時不要按「自動搜尋」或「切換版本」：它們會指到使用者原本的安裝。
- 遊戲卡住或閃退時，強制結束前要先問使用者。
- 使用者沒有要求就不推送。
- 原版比對測試的做法（2026-11-15 用過兩次）：
  1. 把私人 Steam 副本的 `savegame/`、`dxcfg.ini`、`Options.ini` 備份到 `.local/p10-misc/<名稱>/backup`。
  2. 放入 session-29 存檔；`dxcfg.ini` 改成 `presentation=windowed`、`display=1600x1200@120`。
  3. 在背景執行副本裡的原版 `u9.exe`（不注入），等它結束。
  4. 還原，並用 `diff`／`cmp` 和雜湊確認。

## 3. 已完成的工作（P10～P11）

### 3a. P10-1 misctext UI（報告：`reports/P10_1_MISCTEXT.md`；研究：`docs/research/U9_MISCTEXT_UI_PATH.md`）

- 做法 M1：misctext 字串前面加 4 bytes 標記（`0x8F` 加 3 位 base-94 數字）。在排版 0x5C7620、繪字 0x5C5C00、訊息容器複製 0x41BC61 三處就地移除。
- 遊戲內已確認：選項頁、目錄、主選單按鈕、離開遊戲對話框（內縮 8 px）、圖鑑、路牌（亮字加黑框）。
- 刻意保留英文：
  - 力量之語音節（168–193）、咒語（276–311）、秘藥縮寫（316–347）；
  - 日誌目錄的 `TABLE of CONTENTS`；
  - 圖鑑花體字標題（misctext 255）。
- 使用者審閱過的翻譯：BOOKS 1 的標題改為「怪獸／敵人」。

### 3b. P10-2（小字型、時鐘已在 2026-11-12 進遊戲確認；商店尚未）

- 小字型中文縮放（`38a5916`）：字型高度 15–16 用 14 px，≤14 用 12 px。只套用在 misctext UI，日誌、書頁、路牌維持 16 px。
- 商店清單（`4c52ab4`）：商品名稱取 TYPENAME 譯文；店名方案 0xE000 起；runtime 在每個商品名前插入標記。
- 時鐘訊息（`678e474`）：在 0x41BC61 比對英文句型，改寫成中文；單字方案 0xF000 起。

### 3c. P11 影片字幕（研究：`docs/research/U9_MOVIE_SUBTITLES.md`）

- 12 部影片，只有 4 部有對白：intro、samhayne、raven、ambush。其他 8 部經過關閉 VAD 的整片辨識和 15 秒分段強制辨識，都沒有對白；兩個單字候選由使用者聽過確認不是對白。
- 做法 B'（`6494cff`）：runtime 攔截 `CoCreateInstance`（IAT 0x740418）的 FilterGraph，改用自己的播放器播放，以 pl_mpeg（MIT）解碼，用 GDI 畫字幕。GOG 的 `dshow.dll` 不修改。
  - 只有建置輸出裡有 `runtime/movies.tsv` 時才啟用。
- 字幕語言：**只放中文**。使用者在 2026-11-11 決定不做中英並呈，因為英文對白散布有版權問題。
- 檢視工具：`python scripts/movie_review.py <movie> [--scale 2]`。按鍵：空白鍵暫停、←/→ ±5 秒、↑/↓ ±1 秒、PgUp/PgDn 上下一句、R 重新載入。
  - 修改 `movies.jsonl` 後執行 `--cues-only`，再在工具裡按 R。
- 語音辨識的教訓：VAD 會濾掉壓在配樂下的對白（intro 2:14–2:40 整段遺失）。之後一律關閉 VAD，並逐字檢查字幕是否涵蓋每個辨識出的字。
- 本機工具（僅本機）：
  - FFmpeg 9.0.2（winget gyan）；
  - faster-whisper 1.2.1，裝在 `.local/tools/asr-venv`；
  - 辨識稿放在 `.local/movie-asr/`。

### 3d. 影片修正與滑鼠功能（2026-11-11）

- 影片（`5ddc86d`）：
  - 影片視窗蓋滿遊戲畫面；字幕改用快取點陣圖，播放不再延遲。
  - 月之門的開場動畫播放完整（session 17）。
  - Esc 跳過還沒再測。
- 滑鼠功能（IB-1／IB-2，runtime 0.2.40，見 `docs/IMPROVEMENT_BACKLOG.md`）：
  - 游標停留 250 ms 顯示物品名稱。
  - 滾輪翻頁；Ctrl＋滾輪縮放鏡頭。
  - 已修正 Ctrl+X 誤觸離開確認。
  - session 23 遊戲內全部 PASS。
- 堆疊物品名稱「(27) 金幣」（`edbf99f`，runtime 0.2.41）：
  - 遊戲用 `"(%d) %s"` 組標籤，前綴會讓標記比對失敗；
  - 現在允許數量前綴；
  - session 25 PASS。
- 影片 Esc 跳過：session 24 PASS（1.96 秒時由遊戲的按鍵處理停止）。
- 旁白字幕在某些視角消失：只記錄、不修改。證據強，是原版行為，見 `docs/research/U9_NARRATION_CAMERA_VISIBILITY.md`。
- session 24 的「閃退」其實是視窗被最小化，遊戲仍在執行；原因不明，可能是焦點被搶走。
- 啟動頁 `U9Cht.exe`（`3ae3b5f`，runtime 0.2.42，說明見 `runtime/U9ChtFront/README.md`）：
  - 頁面內容：遊戲位置、強化功能開關、字型與日誌排版、解析度。
  - 開始遊戲時套用中文化資料，離開後還原。
  - 解析度只支援 800×600 和 1600×1200，其他大小會讓 runtime 以錯誤 18 結束遊戲。
  - 測試資料夾：`.local/p10-misc/front-release`，遊戲位置指向私人副本。
  - **測試時不要按「自動搜尋」**：它會找到使用者原本的 GOG 安裝。
- Steam 版支援（runtime 0.2.43）：
  - Steam 版 `u9.exe` 就是 GOG 版加上 EA 數位簽章；程式碼、`ddraw.dll`、4 個 FLX 都相同（CONFIRMED）。
  - 啟動器和 runtime 接受兩種雜湊值。
  - 自動搜尋會讀 Steam 遊戲庫（`libraryfolders.vdf`）；同時找到 GOG 和 Steam 版時，由玩家用「切換版本」按鈕選擇。
  - Steam 版的 `dxcfg.ini` 是 `display=desktop`，改解析度時不動它。
  - 私人副本 `.local/steam-game` 已在遊戲內實測，中文化正常（2026-11-12）。
  - **測試時也不要按「切換版本」**：它會指到使用者原本的安裝。
- 「讀檔後再開新遊戲，世界沒有重置」：原版 bug，英文原版也會（CONFIRMED）。只記錄，見 `docs/research/U9_NEW_GAME_AFTER_LOAD.md`。說明檔要提醒玩家先重新啟動遊戲。
- session 28（Steam 副本，作弊鍵走流程）發現兩個問題，已修正（runtime 0.2.44、建置 `out-m25`），尚未進遊戲驗證：
  - 日誌開頭出現「!mH」：日期標頭「Day N:」由介面字串 misctext 0x63 寫入 `diary.txt`，帶著介面記號；顯示日誌時現在也展開這個記號。記號編號跨建置會變，對不上時改用目前的標頭字串（計數 `day_headers`）。
  - 背包藍色書名「Game Controls」沒翻：TYPENAME 5737 原本沒有翻譯紀錄，已補「冒險日誌」。
- session 工具：可用 Steam 副本（接受 Steam 版 u9.exe，執行時改為視窗模式）；所有檢查在寫入前完成；finish 自動使用 start 時記錄的副本。
  - session 28 的 finish 曾誤用預設的 GOG 副本，兩個副本都已復原並驗證；GOG 副本原本的存檔保留在 `session-28/gog-savegame-before-wrong-finish`。
- 開發者提示（2026-11-12～13，runtime 0.2.45，只在按鍵檔有 [CHEAT COMMANDS] 時啟用，即只有測試按鍵檔）：
  - 作弊鍵按下後，畫面上方顯示中文提示（如「法力已補滿」「無敵（開）」）；開關狀態讀自遊戲記憶體。session 30～33 實測正常。
  - 聖者施法時顯示「聖者施展「投石術」」：攔截施法函式 `0x5ED950`。session 33 實測 47 次全部命中（CONFIRMED）。
    - 施法函式有 **6 個**堆疊參數（`ret 0x18`）。先前誤讀成 3 個，session 30 第一次施法就閃退，已修正。
    - 聖者判斷：施法者編號經物件表 `[0x8515D0]+h*12+8` 換成物件，再和玩家記錄 `[[0x98F320]+4]`（是物件指標）比對。
    - 計數輸出保留最近 8 次施法 `recent_casts`（施法者物件、聖者物件、法術編號）。
  - 提示的字預熱失敗時只跳過並記錄（`unwarmed`），不會讓初始化失敗（session 30 第一次啟動曾因此 error 9）。「#」與數字 0～9 目前預熱不起來，原因未查，只影響「法術 #N」備用文字。
- session 29 的「施法後聖者掉進地板、走幾步就閃退（0xC0000005）」：session 32～33 試過多種法術都無法重現，法術仍 UNKNOWN。當時還沒有施法攔截，所以與攔截無關。
- 飛行作弊開了再關，聖者無法移動（session 31）：提示只讀不寫，推測是遊戲內建作弊本身的問題（`0x49B7F0` 關閉時把移動模式設為 0），HYPOTHESIS。待用原版 exe、同一按鍵檔確認。可能與之前不明的「無法移動」有關。
- 法力條上的法術名被截斷（只顯示「地－投」）：已修正並在 session 34 實測完整顯示（CONFIRMED）。
  - 標籤建構函式 `0x41ACE0` 的 strdup（呼叫點 `0x41AD54`）拿到帶記號的 misctext，按記號位元組和窄字位量寬度。
  - 現在複製時換成字位並登記（同 TYPENAME 標籤），寬度由 layout/bounds 點給貼圖寬度。計數 `misc_label_binds`（session 34：23 次，全部來自 `0x41AD54`）。
- 體力／法力數值顯示（2026-11-13，玩家功能 `status_numbers`，預設開啟）：session 35 實測位置與數值正確（CONFIRMED）。
  - 讀聖者屬性記錄 `[0x98F31C]+type*0x13C`：`+0x34/+0x36` 體力、`+0x3A/+0x3C` 法力（鍵名來自 0x555E47 的屬性寫出程式）。
  - 只在介面管理器回報橫條顯示中（`M+0x0B+i`，0 體力、1 法力）時畫；對話／選單時是否隱藏還沒專門確認。
  - 首頁已有勾選框「體力／法力數值」（版面尚未實際目視確認）。
- 傷害／法力飄字（2026-11-13，玩家功能 `floating_numbers`，預設開啟）：session 36 實測正常（CONFIRMED，9 個飄字、無缺字）。
  - 每幀比對聖者體力／法力：扣血紅「-N」、回血 3 點以上綠「+N」、耗魔藍「-N」，從橫條上方往上飄 1.3 秒。
  - 讀不到的幀之後、或上限改變時只重新同步，不飄字。首頁勾選框「傷害／法力飄字」。
- 敵人血條（2026-11-13～15）：session 50 驗證完成（CONFIRMED）。
  - 玩家功能 `target_bar`，預設開啟；首頁勾選框是「敵人血條」。
  - 鎖定目標存在戰鬥管理器 `[0x98F324]`（`0x4F8FD0`／`0x4F1D90`）：
    - `+0x100C` 是鎖定開關。
    - `+0x1008` 是它自己項目陣列的索引，-1 代表沒有目標。
    - 項目 `[P+i*4]+0xC` 直接指向目標的屬性記錄。
  - 屬性記錄表 `[0x98F31C]`：
    - 512 筆，每筆 0x13C，共 `0x27800`。讀檔時重新配置（`0x4F2270`）。
    - 每筆的 `+0x04` 是英文名字，`+0x34/+0x36` 是體力。
    - 記錄格會重複使用。記錄對應的物件是 `[[0x98F320]+4+i*4]`。
  - 物件座標（session 49）：
    - 物件 `+0x04` 是東西向，`+0x06` 是南北向，`+0x08` 是高度。
    - 12 位元，4096 繞回。走路約 150 單位／秒。
    - 物件 `+0x0A` 是記錄編號。
  - 顯示規則：
    - 位置在畫面上方中央、提示文字下方，最多 3 條。鎖定目標排第一（亮紅），其他是暗紅。
    - 每幀比對 512 筆記錄來偵測扣血；同一物件才算。
    - 未鎖定的對象，只在聖者攻擊後 3 秒內才算：近戰（左鍵，遊戲視窗在前景）要在 450 單位內，施法要在 1500 單位內。
    - 上限用該個體觀察到的最高體力。這是使用者的選擇：沒受傷的狼顯示 20/20，記錄裡的上限其實是 30。
    - 被打中的敵人在血條右側飄出紅色傷害；火球打中多隻時各自飄字。
  - 使用者決定不做：
    - 名稱顯示：要在建置時加「英文名稱雜湊 → 中文」對照。
    - 頭頂血條：需要鏡頭投影。
    - 可視範圍和遮擋判斷。
  - 這三個顯示功能的每幀路徑完全不配置記憶體（`e6dc102`）。這是追查閃退時改的，保留。
  - 研究用的部分還留在 runtime：
    - 只讀的計數：`targets_seen`、`recent_hits`、`recent_casts`。
    - 屬性傾印和座標記錄已停用，但程式還在（`dump_record`、`probe_positions`）。
    - 開發用開關：中文化資料夾放 `overlay_draw.off`，就只計算不畫。
    - 正式版前要清理，或改成只在開發模式才有。
- 測試時是視窗化（session 工具改的），但無法拖動視窗：U9 用 `ClipCursor`／`SetCursorPos` 把游標鎖在視窗內，原版行為。可用 Alt+Tab 後 Win+方向鍵移動。

## 4. 用中文手冊校對文本（2026-11-11 完成）

### 4.0 結果

- 手冊三冊（Journal、魔法書、安裝指引）逐頁讀完。比對 241 個詞：一致 80、衝突 123、權威檔沒有 38。
- 使用者逐項裁決（原則與例外見翻譯規範 §1.1）。權威檔改 188 列、新增 35 列；store 改 1,639 筆（text 1,203、TYPENAME 185、BOOKS 173、misctext 72、影片 6），另外 6 筆因譯名變長重新斷行。
- 驗證：
  - validate PASS（error 0）；
  - `python -m unittest discover -s tests` → 166 OK（3 skipped）；
  - 完整建置 `.local/p10-misc/out-m13` PASS（含 `runtime/movies.tsv`）；
  - C++ 沒有變動，未重跑 ctest。
- 本機工作檔（不可提交）在 `.local/manual/`：
  - 逐頁筆記 `manual_terms_notes.md`；
  - 裁決表 `DECISIONS.csv`、`MISSING.csv`；
  - 套用腳本 `apply.py`、`glossary_update.py`；
  - 修改前後對照 `apply/diff.jsonl`。
- 手冊掃描檔不在 repo 裡，也沒有複製進 `.local/`；位置問使用者。
- 遊戲內待確認：
  - 法術書的儀式清單：標籤變長，例如「重聚肉體與靈魂儀式」9 個字，要看會不會超出。
  - 狀態頁：例如「長程武器」。

以下是校對前的原始計畫，保留作紀錄。

使用者已取得當初台灣發行的中文手冊。依 `translation/TRANSLATION_GUIDE.md` §1，它是**第 1 順位權威**，高於 U8／U7 系列譯名。

### 4.1 手冊的處理規則（AGENTS §17）

- 手冊掃描檔、照片、OCR 全文**只放在本機**，例如 `.local/manual/`（已在 `.gitignore` 涵蓋的 `.local/` 下）。不可提交。
- repo 只能放：
  - 核准的譯名；
  - 出處位置註記，例如「手冊 p.12」；
  - 簡短的事實引用。
- 從手冊採用的譯名，要在 `U9_TERMS.csv` 的 notes 註明出處頁碼。之後可以標成「官方譯名」，這是唯一可以這樣標示的來源。

### 4.2 建議流程

1. **整理手冊譯名**：列出手冊中的人名、地名、美德、咒語、秘藥、物品、職業、介面用語，和 `U9_TERMS.csv` 比對，分成三類：一致、衝突、權威檔沒有。
2. **衝突交給使用者決定**：手冊譯名和已核准的 U8／U7 譯名不同時，不可自行替換。整理成表請使用者裁決，因為這會影響全部 store。
3. **更新權威檔**：`python scripts/update_terms.py`（用法見翻譯規範 §2.3）。
4. **找出受影響的譯文**：用 `grep` 搜尋 store 中的舊譯名，列出受影響的筆數與 key。
5. **改譯文**：
   - 匯出 workset：`python scripts/export_translation_workset.py --resource <資源> --existing-only --output .local/translation-work/<名稱>.jsonl`（含英文原文，只能放 `.local/`）；
   - 修改 target；
   - 匯入：`python scripts/import_translation_workset.py --game-dir .local/runtime-game <workset>`。可先加 `--dry-run` 預覽。
   - 注意：匯入前要確定 store 自匯出後沒有被改過，否則會衝突，要重新匯出。
6. **驗證**：
   - `python scripts/validate_translation.py`；
   - `python -m unittest discover -s tests`；
   - 完整建置 `python scripts/build_localization.py --game-dir .local/runtime-game --font .local/p5-highres/font/NotoSansCJKtc-Regular.otf --output .local/p10-misc/out-m13`。
7. **狀態**：使用者校對通過的條目才改為 `review` 或 `approved`。不可覆寫 `approved` 的譯文而不說明理由（AGENTS §16）。

### 4.3 校對時要注意

- 專有名詞加註英文的規則（翻譯規範 §4.7）：
  - 一般 NPC 人名每句都加註；
  - 地名與其他專有名詞，在每個對話檔第一次出現時加註；
  - 聖者、不列顛王、守護者與同伴只在第一次加註；
  - misctext 不加註。
- 保留控制碼、`%s`、換行、`{{gargish:N}}` 佔位符（AGENTS §18）。validate 會檢查。
- 影片字幕 `movies.jsonl` 裡有 8 句是 `review`。例如「黑暗之鏡」不在權威檔裡；如果手冊有這個詞，就以手冊為準。

## 5. 尚未進遊戲測試的項目（需要使用者說「準備好了」）

用第 0 節的指令（Steam 副本、`out-m26`）。

- 翻譯畫面還沒確認：
  - 商店買／賣清單；
  - 存讀檔頁；
  - 法術書：儀式清單的長標籤，例如「重聚肉體與靈魂儀式」，看會不會超出；
  - 狀態頁：新用語，例如機敏、神力、長程武器、生命力。
- 首頁：新增的三個勾選框（體力／法力數值、傷害／法力飄字、敵人血條）還沒實際目視確認版面，視窗高度現在是 818。
  - 開 `.local/p10-misc/front-release/U9Cht.exe` 確認。不要按「自動搜尋」或「切換版本」。
- 按鍵設定頁（`front-release` 裡已是新版 `U9Cht.exe`）：
  - 實際操作：新增、移除、重複鍵變紅、儲存、載入原版預設、返回。
  - 存檔後進遊戲，確認改過的鍵有效、沒有跳出「keymap file」錯誤訊息框。
  - 注意：存檔會在私人副本留下 `U9CHT.kmp` 並改 `Options.ini`；測完要還原（session 工具會還原 `Options.ini`，`U9CHT.kmp` 要手動刪）。
  - 順便確認 A 鍵（`abort_targeting`）在鎖定目標時到底有沒有作用。
  - **第一次實測（2026-11-15 13:34，首頁啟動、開新遊戲、WASD 加到移動）：進遊戲後完全無法移動，滑鼠右鍵也不行；沒有錯誤訊息框。原因 UNKNOWN。**
    - 同一場首頁被 Defender 隔離；存出的 `U9CHT.kmp` 格式檢查不出問題；記錄沒有例外。
    - 滑鼠移動不經過按鍵檔，而且以前用原版按鍵檔也出現過原因不明的「無法移動」，所以還不能歸因於按鍵檔。
    - 下一步：用 session 工具做對照（同一存檔，`--keymap` 用這份按鍵檔 vs 不加）。按鍵檔副本在 `.local/p10-misc/keymap/U9CHT-user-20261115.kmp`。
    - **session 57（session 工具、session-29 存檔、這份按鍵檔）：方向鍵、WASD、右鍵移動、B、J、Alt+S 全部正常**（CONFIRMED）。首頁存出的按鍵檔沒問題；首頁那場無法移動的原因仍 UNKNOWN（差別：首頁啟動、Defender 隔離、可能是開新遊戲）。
    - **session 58（session 工具、同一按鍵檔、開新遊戲）：WASD 正常**。開新遊戲本身也重現不了。
    - 這和 11-12 記錄過一次的「讀檔後開新遊戲不能移動」相同，見 `docs/research/U9_NEW_GAME_AFTER_LOAD.md`（已補 11-15 的紀錄）。原因仍 UNKNOWN；下次再發生時先按一下 X 再放開，看能不能恢復（測游標停留功能的假設）。
    - 要再用首頁重現，得先讓 Defender 不再隔離 `U9Cht.exe`（使用者自行在 Windows 安全性處理）。
- 體力／法力數值和敵人血條：開啟對話、背包、日誌、選單時，會不會跟著橫條一起隱藏，還沒專門確認。
- 飛行作弊開了再關就無法移動：用原版 exe 加同一份按鍵檔確認是不是原版行為（作法見第 2 節）。
- session 11 播放影片時截圖閃退（0xC0000005，原因 UNKNOWN）：還沒和原版比對。

## 6. 其他待辦

- **防毒誤判（2026-11-15，發布前必須處理）**：Microsoft Defender 在首頁按「開始遊戲」後 3 秒把 `front-release\U9Cht.exe` 判為 `Behavior:Win32/DefenseEvasion.A!ml` 並隔離。
  - 遊戲本身照常執行並正常結束（結束代碼 0），但首頁被移除，所以離開遊戲後**沒有還原**四個 FLX；已依收據手動還原並驗證雜湊。
  - 觸發的是哪個行為還不知道（UNKNOWN）。11-12 用首頁啟動、以及 session 工具啟動都沒被擋。
  - 影響：玩家電腦上也可能發生，而且會留下未還原的遊戲檔（下次開首頁時 `recover()` 會處理，前提是首頁還在）。
  - 已改善（2026-11-15）：首頁傳 `--restore-backup` 給啟動器，啟動器在遊戲結束時就先還原；首頁被移除時檔案也會還原。合成資料測試通過，尚未在真實遊戲走過一次。
  - 防毒誤判本身還沒處理（回報誤判、程式碼簽章、說明檔）。

- 啟動頁下一步：在玩家電腦上建置 `data/`。
  - 需要附帶 Python（embeddable 版）和 `u9cht` 套件，從玩家自己的遊戲檔建置。
  - 建置出的 FLX 含英文原文，不能散布。
- 發布打包流程：還沒規劃。
- 說明檔 `說明.txt`：要寫進「開新遊戲前請先重新啟動遊戲」（原版 bug）。
  - 也要寫：「鎖定敵人時死亡，讀檔前先解除鎖定（T 或 A），否則遊戲會閃退」（原版 bug）。

- 效能（2026-11-12 使用者提出，只記錄）：在現代電腦上仍不順。
  - GitHub 上沒找到 U9 專用的效能專案；社群建議都是 `Options.ini` 調整（`StreamDuration=3000`、`DeadPoolSize=3200`、裁切距離），未驗證。
  - 我們的 runtime 會寫大量診斷紀錄（一場約 1.4 萬筆 `glyph_cache_warm`），可能有負擔。
  - 下一步：同一存檔、同一路線比較三種情況：原版、中文化、中文化但降低紀錄量。先問清楚是低幀率、規律頓挫，還是轉視角卡。
- 平台支援（2026-11-13 使用者提出，只記錄，之後再決定）：
  - U9 本身只有 Windows 版（GOG／Steam 都是），從未出過 Mac 版。
  - 中文化（`U9Cht.exe`、launcher、注入 DLL、x86 inline hook、GDI／waveOut）也只能在 Windows 上執行。
  - Release 說明暫定寫「支援 Windows 10/11（GOG／Steam 版）」。
  - Mac 可能的路徑（皆未驗證，HYPOTHESIS）：
    - Parallels + Windows 11 ARM（x86 模擬）：機會最高。
    - CrossOver／Wine：DLL 注入、hook、ddraw/Glide 包裝、影片與音效要逐項驗證。
    - 原生 Mac 版：不可行（需重寫引擎，超出專案範圍）。
  - 若有 Mac 測試者：先在 Parallels 跑完整檢查（首頁、啟動、對話、日誌、影片字幕、存讀檔），通過才標成「已驗證」。
- 手把（2026-11-13 確認，STRONG EVIDENCE）：U9 原生不支援搖桿／手把。
  - `u9.exe` 雖載入 DirectInput，但沒有任何搖桿相關字串；keymap 只能設定鍵盤與滑鼠。
  - 替代方案：Steam Input，或 JoyToKey／AntiMicroX 把手把對應成鍵盤與滑鼠（未實測）。
  - 自己加手把支援需要新 hook，暫不建議。
- 被怪物打敗後讀檔閃退（session 38，0xC0000005）：session 39 重現不出來，原因不明（UNKNOWN）。
  - session 40 再次發生（聖者死亡後讀檔）。當機記錄：`u9.exe+0x1E0C15`（`0x5E0C15`，鏈結串列尾端插入，寫入損壞節點 `[eax+4]`，eax=0x2EE70E20），堆疊 0x55FA40、0x4CBB2E、0x4C9B6C、0x574EF1…。當機點在遊戲程式內、不在 runtime 或任何 hook 內；損壞來源未知。
  - 讀檔時遊戲會釋放並重新配置屬性記錄表（0x4F2270）。
  - session 41、45 也同樣閃退（同一位置、同一堆疊），可穩定重現。
  - 原版測試（2026-11-15，私人 Steam 副本、未修改英文 u9.exe、session-29 存檔、無注入）：死亡後讀檔**沒有閃退**，正常離開（結束代碼 0）。只測一次，但與中文化下幾乎每次都閃退對比，STRONG EVIDENCE：問題與中文化有關。副本存檔與設定已還原（測試後的存檔在 `.local/p10-misc/vanilla-death-load/after-savegame`）。
  - 二分法：session 51（三個新功能關、原本按鍵檔）死亡後讀檔**不閃退**；session 52（三個新功能開、原本按鍵檔）**閃退**（`u9.exe+0x15FBEB`，同一區）。問題在 status_numbers／floating_numbers／target_bar。
  - 推測（HYPOTHESIS）：這些功能每幀配置／釋放字串（與遊戲共用同一個堆積），重用了遊戲在死亡／讀檔時「釋放後仍使用」的記憶體。畫面寫入有 DirectDraw 鎖定與驗證，較不可能。
  - 修正（待驗證）：這三個功能的每幀路徑改用固定緩衝區，完全不配置記憶體；研究用的屬性傾印與座標記錄已停用。
  - session 53 仍閃退；session 54（只計算不畫）使用者發現：**未鎖定**死亡讀檔正常，**鎖定敵人**時死亡讀檔閃退。
  - **結論（CONFIRMED）：原版錯誤。** 原版 exe 鎖定敵人後死亡再讀檔也閃退（segmentation fault）。與中文化無關。見 `docs/research/U9_LOCKED_TARGET_DEATH_LOAD_CRASH.md`。說明檔要提醒：讀檔前先解除鎖定（T／A），或鎖定中死亡後重新啟動遊戲。
  - 已加繞過方法（使用者同意）：聖者死亡且鎖定中時，呼叫遊戲自己的解除鎖定 `0x4F90C0`（在畫面更新流程、runtime 鎖之外）。session 55 曾因在鎖內呼叫而死結；session 56 驗證兩次死亡讀檔都正常（CONFIRMED）。有了繞過方法，說明檔的提醒可視情況省略。已加當機記錄（vectored handler，只記錄不攔截，`exception` 事件，最多 64 筆），再遇到時看最後一筆的模組與位址。
- 對話時直接跳到下一句（2026-11-13 使用者提出，只記錄，待研究）：先查原版有沒有跳過鍵或滑鼠點擊跳過，再看語音／字幕的推進點能否安全提前。
- 避頭點（行首標點）規則、英文單字中途斷行。
- 用 ✕ 關窗時的 0xC000041D 與原版比對。
- 商店清單 QA：還有 14 個物品和 7 個註解的加註警告（見 P10 文件）。
- 本機翻譯工作檔：`.local/translation-work/`（p10-misc、p10-shops、p9-books）。
