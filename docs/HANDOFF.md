# U9-CHT Handoff（2026-11-17）

給下一個工作階段（人或 AI Agent）的交接。
開工前仍須依 `AGENTS.md` 讀取 `AGENTS.md` 與 `ULTIMA_IX_CHT_MASTER_PLAN.md`。

**這一波（2026-11-15～16）做完的事**，細節見第 3e 節：
- 首頁「按鍵設定」頁，依 `u9.exe` 讀按鍵檔的規則重做，作弊鍵也能設定。
- 遊戲中即時套用按鍵，加上 Ctrl+Shift+K 叫出設定頁、「回到遊戲」切回。
- 啟動器在遊戲結束時自己還原遊戲檔（`--restore-backup`）。
- **改用代理 `dsound.dll` 載入 runtime，不再注入**；Defender 不再攔截（這台電腦、一次）。
- 跳躍鍵自動攀爬（首頁開關，預設關）。

**2026-11-16～17 追加**，細節見第 3f 節：
- 幀率低的主因找到並修正：中文改經遊戲的 Direct3D 裝置繪製，不再每幀鎖定畫面緩衝區。
- 評估了 U9GL（OpenGL 渲染器專案）：只當參考，目前不能和我們同時使用。

---

## 0. 新對話開工清單

1. 讀 `AGENTS.md`、`ULTIMA_IX_CHT_MASTER_PLAN.md`，以及本文件第 1、2 節。
2. 建置 runtime（從 repo 根目錄執行）：
   ```sh
   "/c/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" --build .local/p10-misc/build --config Release
   ```
   - 新增原始檔或測試檔後，要先執行 `cmake -S runtime -B .local/p10-misc/build`。
   - C++ 測試：在 `.local/p10-misc/build` 執行同一個資料夾裡的 `ctest.exe -C Release`（Git Bash 的 PATH 上沒有 `ctest`）。
3. Python 測試：`python -m unittest discover -s tests`。
4. 進遊戲測試用 Steam 私人副本，使用者說「準備好了」之後才執行：
   ```sh
   python scripts/run_p8_0_session.py start --session .local/p10-misc/session-NN --label "..."      --game-dir .local/steam-game --build .local/p10-misc/out-m26 --runtime .local/p10-misc/build/Release      --config .local/p9-2/u9cht.ini --preserve-saves --seed-saves .local/p10-misc/session-29/final-savegame      --load proxy --keymap .local/p10-misc/keymap/U9CHT-TEST.kmp
   python scripts/run_p8_0_session.py finish --session .local/p10-misc/session-NN
   ```
   - 下一個 session 編號從 **65** 開始。
   - 較新的測試存檔：`.local/p10-misc/saves/2026-11-17-front`（使用者 2026-11-17 從首頁玩到的進度，最新是 `u9game13.sav`；雜湊在 `saves/2026-11-17-front.sha256`）。要從這裡開始，把 `--seed-saves` 換成這個資料夾。
   - `--load proxy`：用代理 `dsound.dll` 載入（和首頁相同）。不加就是舊的注入方式（啟動器的預設），留作對照。
   - 不加 `--keymap` 就用 `Options.ini` 目前指定的按鍵檔（見第 1 節「私人副本」），沒有作弊鍵。
   - 要關掉個別功能，就用另一個設定檔，加上 `[features]` 區段。範例：`.local/p10-misc/bisect/u9cht-newfeatures-off.ini`。
   - session 工具會讓遊戲以視窗模式執行；經首頁啟動則是全螢幕。
5. 首頁測試：`.local/p10-misc/front-release/U9Cht.exe`（使用者自己開）。
   - 重新建置後，要把 `U9Cht.exe`、`U9ChtLauncher.exe`、`U9ChtRuntime.dll`、`U9ChtProxy.dll` 四個檔從 `build/Release` 複製過去。首頁開著時無法覆蓋 `U9Cht.exe`，先請使用者關掉。
   - 首頁的記錄在 `front-release/logs/`。
6. Runtime 版本號：
   - 改 runtime 名稱時，三處要一起改：`runtime/common/version_manifest.h`、`u9cht/l10n/runtime_pack.py`、`runtime/CMakeLists.txt`。
   - 改了以後要重建中文化資料，因為 pack 內嵌 runtime 名稱。
   - 目前仍是 `0.2.45`。這兩波加的功能都沒有升版號。

## 1. 目前狀態一覽

- 分支：`p7-localization-pipeline`，已推送到 `origin`（2026-11-16，`a44ee4b`）。
  - 本機備份分支 `backup/p7-before-gargish-rewrite` **不可推送**。這是 2026-11-11 改寫歷史前的備份。
- 工作樹：只有不可加入的未追蹤檔：`.vscode/`、`83e7052b…png`、`tools/u9xsb6bt2a*`、`tools/uixtools.zip`。
  - `tools/u9xsb6bt2a/` 是第三方工具 Extended Setup（著作權屬原作者），不在 `.gitignore` 裡，**不可加入**。
- 私人遊戲副本，沒有進行中的 session：
  - Steam：`.local/steam-game`。最近的測試都用它。資源檔是原版，沒有殘留的 `dsound.dll`。
    - **注意：`Options.ini` 目前是 `Current_Keymap=U9CHT.kmp`**，遊戲資料夾裡有使用者 11-15 15:45 從首頁存的 `U9CHT.kmp`。這是使用者的首頁測試狀態，不要擅自改回。
    - 因此 `session-56/backup/Options.ini` 不再是可用的比對基準。要比對時以 session 工具 start 時自己備份的 `backup/` 為準。
  - GOG：`.local/runtime-game`。建置中文化資料時用它的原版檔。
- 測試：
  - C++：15/15（2026-11-17）。
  - Python：166 OK（3 skipped）。
- Runtime：`U9ChtRuntime/0.2.45-p11`，建置在 `.local/p10-misc/build/Release`。
- 中文化資料最新建置：`.local/p10-misc/out-m26`。
- 測試存檔：`.local/p10-misc/session-29/final-savegame`（Steam 副本）。session 30～61 都從這裡開始，沒有寫回。
- 首頁測試資料夾：`.local/p10-misc/front-release`，四個程式都是最新版（2026-11-16）。
- Microsoft Defender：`.local/p10-misc` 的排除**已拿掉**（使用者 2026-11-16）。代理載入沒被攔截；舊的注入方式會被攔截。

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
- 改遊戲設定檔（`Options.ini`、`dxcfg.ini`、按鍵檔）不要用 Git Bash 的 `sed -i`：它會把 CRLF 改成 LF（2026-11-15 發生過）。要還原就從驗證過的備份複製，要修改就用 Python 二進位模式，改完用 `cmp` 或雜湊確認。
- 首頁正在執行時不要關掉它；要覆蓋 `front-release` 的檔案前先請使用者關閉。
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

### 3e. 按鍵、載入方式與操作改善（2026-11-15～16）

研究與說明文件：
- `docs/research/U9_EXTENDED_SETUP_TOOL.md`：第三方工具 Extended Setup 的功能、`u9.exe` 讀按鍵檔的規則（追過程式碼）、鍵盤移動一次只能一個方向、即時套用的做法。
- `docs/research/U9_PROXY_DLL_LOADING.md`：代理 `dsound.dll` 的研究、實作與 Defender 結果。
- `docs/research/U9_JUMP_CLIMB.md`：跳躍與攀爬的機制、跳躍鍵自動攀爬的做法與結果。
- `runtime/U9ChtFront/README.md`：首頁、按鍵設定頁、快捷鍵、載入方式。

**按鍵設定頁**（`runtime/U9ChtFront/keymap.h`／`keymap.cpp`，測試 `U9ChtKeymapTests`）：
- 6 個分頁、97 個指令（含作弊）；一個指令最多 4 個鍵；按＋後直接按鍵；重複鍵標紅（顯示遊戲實際會執行哪一個）。
- 存到遊戲資料夾的 `U9CHT.kmp`，只改 `Options.ini` 的 `Current_Keymap` 一行；不動 `Default.kmp`。
- 遊戲讀按鍵檔的限制（寫檔時都遵守）：每行去空白後 < 64 bytes、鍵名與指令名 < 32；每區段 4,096 bytes；Esc、F1～F12 保留；不認得的鍵名會跳訊息框。
- 遊戲內驗證：session 57、58（首頁存的按鍵檔有效）。

**遊戲中即時套用**（`runtime/U9ChtRuntime/keymap_live.h`，`engine.cpp` 的 `poll_keymap`／`reload_keymap`，測試 `U9ChtKeymapLiveTests`）：
- 每 0.5 秒看 `Options.ini` 和按鍵檔有沒有變，穩定 1 秒且沒有按鍵按著時，用遊戲自己的函式清空按鍵、補回內建綁定、重讀檔案。
- 檔案裡有遊戲讀不懂的行就拒絕，沿用舊按鍵。開發用關閉開關：資料夾放 `keymap_live.off`。
- 遊戲內驗證：session 60（直接換檔）。**經首頁「儲存按鍵」後生效還沒測過。**

**Ctrl+Shift+K**：遊戲中叫出首頁的按鍵設定頁；「返回」變成「回到遊戲」，會先還原縮小的全螢幕遊戲。首頁實測通過。設定頁不允許把 Ctrl+Shift+K 指定給遊戲指令。

**啟動器負責還原**：首頁傳 `--restore-backup`，啟動器在遊戲結束時就還原四個 FLX。首頁實測多次通過（`restore_after_game ok`）。

**代理 `dsound.dll` 載入**（`runtime/U9ChtProxy/`，測試 `U9ChtProxyForwardTests`）：
- 啟動器 `--load proxy`：把 `U9ChtProxy.dll` 放進遊戲資料夾當 `dsound.dll`，用環境變數 `U9CHT_LAUNCH` 指向啟動檔，正常啟動 `u9.exe`；代理在遊戲進入點載入 runtime 並回報結果；遊戲結束後移除。
- 沒有環境變數時代理只轉接 DirectSound，從 Steam／GOG 直接開遊戲完全不受影響。
- 遊戲資料夾已有別人的 `dsound.dll` 時拒絕啟動，不覆蓋也不刪除；只處理帶我們標記的檔案。
- 驗證：session 61（中文、聲音正常、結束後移除）；首頁實測，Defender 拿掉排除後沒有攔截。

**跳躍鍵自動攀爬**（`FeatureSmartJump`，`u9cht.ini` 的 `smart_jump`，首頁勾選框，預設關）：
- hook 聖者自己的跳躍函式 `0x4193A0`；跳之前先呼叫遊戲自己的攀爬函式 `0x57EF00`，開始攀爬就不跳。
- 首頁實測通過（攀爬 3、跳躍 5、例外 0）。

**runtime 的唯讀診斷**：`movement_probe`（每場記一次聖者攀爬／跳躍函式位址）。研究用，正式版前可移除。

### 3f. 效能與 U9GL 評估（2026-11-16～17）

文件：`docs/research/U9_PERFORMANCE.md`、`docs/research/U9GL_REFERENCE.md`。

**幀率低的主因（CONFIRMED）**：體力／法力數值、飄字、敵人血條讓幾乎每一幀都要合成中文，而合成要鎖定後備緩衝區。
鎖定一次 7～10 ms，和範圍無關（session 62：合成時間的 97% 在 Lock 呼叫裡）。診斷記錄不是原因。

**修正：經 Direct3D 裝置繪製**（`runtime/U9ChtRuntime/gpu_overlay.h`／`.cpp`，測試 `U9ChtGpuOverlayTests`；使用者授權 2026-11-16）：
- 字形放進一張 1024×1024 貼圖，每個繪製指令畫成半透明矩形。裝置是 `[backend+0x3FC3BC]`。沒有新的遊戲 hook。
- 游標壓在中文字上時，把字切成條狀矩形避開被蓋住的像素。
- 會動到的繪圖狀態先讀出、畫完還原。任何一步失敗就退回鎖定路徑；連續失敗 8 次、例外或還原失敗就整場停用。
- **預設開啟**（使用者決定 2026-11-17）。中文化資料的 `runtime/` 資料夾放 `gpu_overlay.off` 可強制走鎖定路徑。
- 驗證：session 63、64（視窗模式 1600×1200）。每幀 0.6 ms（原本 8.7 ms），session 64 沒有任何一幀鎖定，錯誤 0。使用者目視確認字形、游標、遊戲畫面都正常，體感較順。
- `runtime_counters` 新欄位：`lock_ticks`、`unlock_ticks`、`locked_pixels`、`lock_frames`、`gpu_*`。

**還沒驗證**：全螢幕（經首頁啟動）、800×600、原版在同一場景的幀率。

**U9GL**（`theGreyWanderer-uc/tgwUltima9GL`，0.5.8）：
- 和我們衝突：它的載入器也叫 `dsound.dll`；它要求 Glide 再換成自己的渲染器，我們的中文繪製綁定 DX7 渲染器。
- 授權是非商業 copyleft，不可複製程式碼；位址表可當線索，要自己驗證。
- 使用者實測它沒有比較順。結論：只當參考，說明檔要寫「目前不能同時使用」。
- 本機副本在 `.local/upstream/tgwUltima9GL`。

### 3g. 首頁與對話功能（2026-11-17～18）

- 首頁標題副標改為「羽化昇天」（譯名權威檔的台灣官方副標）。
- **視窗化**勾選框：開始遊戲時把 `dxcfg.ini` 改成視窗（`display=desktop` 改成遊戲解析度），取消時逐位元組還原。使用者確認正常。
- **對話字距**（0～2）、**對話行距**（0～8）：`u9cht.ini` 的 `[dialogue]`，協定版本 4。行距用新 hook `0x5C7E50`（使用者授權）。
  多行對白確認有效；**對話選項之間的距離沒有變**，使用者決定先不處理。見 `docs/research/U9_DIALOGUE_SPACING.md`。
- **世界地圖放開 Ctrl 就關**：原因是使用者電腦上的按鍵對應軟體（Copilot 鍵對應成 Ctrl），放開時送出 Win 鍵和鍵碼 0xFF，
  地圖對任何按鍵都會關。現在 Win 鍵與 0xFF 不交給遊戲（遊戲本來也綁不到它們）。使用者確認修好。見 `docs/research/U9_WORLD_MAP_HOVER.md`。
- **點擊任意處跳過對白**（首頁開關，預設開啟，`FeatureClickSkip`）：hook 對白項目 `0x41C070`／`0x41C190` 與選項選單
  `0x41C3D0`／`0x41C590`（使用者授權）。使用者確認四項正常。見 `docs/research/U9_CLICK_SKIP.md`。
- 原版本來就有、可以寫進說明檔的操作：**按 Enter 跳下一句對白**；**按數字鍵 1～N 直接選對話選項**（後者是靜態分析，未實測）。
- **Esc 開啟日誌目錄**（首頁第 10 個開關，預設開啟，`FeatureEscContents`）：沒有開著的書或視窗時，Esc 以名稱 `journal_contents`
  呼叫遊戲的日誌指令處理程式 `0x42DE80`，它對不認得的名稱開第 0 頁＝目錄。不改遊戲程式碼。session 65 使用者確認三項 OK
  （計數：Esc 9 次、開目錄 3 次、書開著時不動作 6 次）。J 回到上次那頁是原版設計。見 `docs/research/U9_JOURNAL_CONTENTS.md`。
- **密技「金幣增加一千」**（使用者授權）：按鍵檔 `[CHEAT COMMANDS]` 的 `add_gold`（首頁按鍵設定作弊頁可設；測試按鍵檔是 Ctrl+Shift+G）。
  用遊戲的屬性設定 `0x4C56F0` 改背包金幣物品（種類 908）的屬性 17，上限 9999；身上沒有金幣時只提示。session 65 使用者確認 OK
  （加 10 次、已滿 2 次、失敗 0）。見 `docs/research/U9_GOLD.md`。背包的金幣物品名同時改為「金幣」（原「黃金」）。
- 狀態頁（Ctrl+Z）本來就顯示金幣與箭的數量，使用者確認可見。
- **游標停留顯示名稱打斷「走過去坐下」**（新遊戲吉普賽人的椅子，劇情卡住）：模擬的 X 鍵取消了動作。修正：真實點擊後，游標離開點擊處 8 像素前不送 X（`click_hold`）。使用者確認修好。見 `docs/research/U9_KNOWN_ISSUES.md` 第 2 節。
- GOG Galaxy 雲端存檔：把自訂執行檔設成 `U9Cht.exe` 後會同步，使用者測試成功；Steam 未測。見 `docs/release/RELEASE_PROCESS.md`。
- 截圖時出現「Unable to create Primary Surface in Video Memory!」後遊戲結束：已記錄，原因未查，見 `docs/research/U9_KNOWN_ISSUES.md`。

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
- **新的繪製路徑在全螢幕（經首頁啟動）和 800×600 下是否正常**：字形、游標、遊戲畫面、切換視窗後回來（貼圖遺失後的復原沒實測過）。
- 首頁版面：強化功能現在有 10 個勾選框（列距 30），視窗高 914，截圖檢查過、使用者也用過，但還沒在高 DPI 螢幕上看過。
- 這一波的功能，還沒測到的部分：
  - **遊戲中經首頁「儲存按鍵」後即時生效**（Ctrl+Shift+K → 改鍵 → 儲存 → 回到遊戲 → 新鍵有效）。
  - 跳躍鍵自動攀爬：移動中起跳、較長時間遊玩；以及「跳躍被拒絕但攀爬允許」的那一種情況（`0x51C0F0`）。
  - A 鍵（`abort_targeting`）：靜態分析顯示 `u9.exe` 沒有這個指令，鎖定目標時按 A 很可能沒作用。確認後要改 `docs/testing/KEYS_zh-TW.md` 和說明檔的「T 或 A」。
- 「不能移動」（原因 UNKNOWN，至今兩次，都是開新遊戲之後）：見 `docs/research/U9_NEW_GAME_AFTER_LOAD.md`。按鍵檔已排除（session 57、58）。再發生時請使用者先按一下 X 再放開，看能不能恢復（測游標停留功能的假設）。
- 體力／法力數值和敵人血條：開啟對話、背包、日誌、選單時，會不會跟著橫條一起隱藏，還沒專門確認。
- 飛行作弊開了再關就無法移動：用原版 exe 加同一份按鍵檔確認是不是原版行為（作法見第 2 節）。
- session 11 播放影片時截圖閃退（0xC0000005，原因 UNKNOWN）：還沒和原版比對。

## 6. 其他待辦

- **防毒**：
  - 2026-11-15 Defender 把用注入方式啟動的首頁判為 `Behavior:Win32/DefenseEvasion.A!ml` 並隔離。
  - 2026-11-16 改用代理 `dsound.dll` 後，拿掉排除測試，沒有被攔截（這台電腦、一次）。
  - 還可以考慮：把啟動器的預設改成 `--load proxy`、程式碼簽章、每個發布版本送 Microsoft 檢查、說明檔提醒。

- 在玩家電腦上建置 `data/`：改用差異檔，不附 Python（見下一項）。建置出的 FLX 含英文原文，不能散布。
- 發布打包（2026-11-18，使用者決定：差異檔＋首頁套用、先做內部測試包、只在本機打包）：
  `scripts/package_release.py`，格式與流程見 `docs/release/PATCH_FORMAT.md`、`docs/release/RELEASE_PROCESS.md`。
  包裡沒有 FLX；首頁第一次開始遊戲時從玩家的原檔產生 `data/static/`。說明檔 `release/說明.txt`。
- 說明檔 `說明.txt`（已寫，`release/說明.txt`）：已含「開新遊戲前請先重新啟動遊戲」（原版 bug）與防毒說明。
  - 也要寫：「鎖定敵人時死亡，讀檔前先解除鎖定（T），否則遊戲會閃退」（原版 bug）。A 鍵很可能沒作用，見第 5 節。runtime 已有繞過方法，這段提醒可視情況省略。
  - 也要寫：防毒軟體可能攔截，以及處理方法。

- 效能（2026-11-12 使用者提出）：主因已修正，見第 3f 節。以下是當時的記錄，其中「診斷記錄可能有負擔」已排除。剩下的幀率起伏（30～120 fps 隨場景）要先量原版才知道是不是遊戲本身。
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
