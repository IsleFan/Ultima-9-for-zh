# 創世紀 IX：羽化昇天　繁體中文化（U9-CHT）

**Ultima IX: Ascension** 的非官方繁體中文化，免費提供。

這個中文化不含任何遊戲檔案：玩家用自己的正版遊戲（GOG 版或 Steam 版），中文化在你的電腦上從原檔產生中文資料，遊戲執行時才把中文顯示出來，離開遊戲就恢復原狀。

**[下載最新版本（GitHub Releases）](https://github.com/pmanyeh/Ultima-9-for-zh/releases/latest)**

| | |
| --- | --- |
| ![開場影片的中文字幕](https://github.com/pmanyeh/Ultima-9-for-zh/releases/download/v1.0.0/01-movie-subtitles.jpg) | ![角色對白](https://github.com/pmanyeh/Ultima-9-for-zh/releases/download/v1.0.0/02-dialogue.jpg) |
| ![日誌](https://github.com/pmanyeh/Ultima-9-for-zh/releases/download/v1.0.0/05-journal.jpg) | ![路標與物品名稱](https://github.com/pmanyeh/Ultima-9-for-zh/releases/download/v1.0.0/07-signpost.jpg) |

---

## 中文化了什麼

- **文字**：對話（約 7,400 句）、物品名稱（約 2,000 個）、書本與日誌、商店清單、介面與系統訊息。
- **影片字幕**：開場與劇情影片加上中文字幕。
- **路標**：路上的地名牌畫成中文。
- **譯名**：以台灣出版的中文手冊為第一依據，其次延續系列作的台灣譯名，整理成[譯名表](translation/glossary/U9_TERMS.csv)。

譯文目前是**初稿**，還沒有逐句人工校對。文本很多，需要大家一起幫忙（見[一起讓中文化更好](#一起讓中文化更好)）。

## 強化功能

原版沒有、可以在中文化首頁逐項開關的功能：

- 游標停在物品上就顯示名稱，不必按住 X
- 滑鼠滾輪翻書、捲動商店清單；Ctrl＋滾輪縮放鏡頭
- 體力／法力數值、傷害飄字、鎖定敵人的血條
- 點畫面任何地方跳過對白；Esc 直接開日誌目錄
- 按住 F 標出附近可以拿的東西
- 跳躍鍵自動攀爬（預設關閉）
- 按鍵設定頁：所有遊戲按鍵都能在首頁修改，遊戲中按 Ctrl+Shift+K 也能叫出來
- 支援 800×600 到 1600×1200 的 4:3 解析度與視窗模式

## 怎麼玩

需要：正版 Ultima IX: Ascension（GOG 或 Steam，遊戲版本 1.19f）、Windows 10／11、約 400 MB 硬碟空間。

1. 從 [Releases](https://github.com/pmanyeh/Ultima-9-for-zh/releases/latest) 下載 zip，解壓到任何地方（不要放進遊戲資料夾）。
2. 執行 `U9Cht.exe`（中文化首頁），它會自動找到遊戲。
3. 按「開始遊戲」。

每次都從首頁進入遊戲；直接執行 `u9.exe` 會是英文原版。存檔和英文版共用。詳細說明見 zip 裡的 [`說明.txt`](release/說明.txt)。

**Mac**：遊戲只有 Windows 版，但可以用 Wine 執行（建議 Sikarugir 的 Wine 10.0），步驟見 [Mac 說明](docs/release/MAC_TEST_GUIDE.md)。目前只有少數玩家試過，歡迎回報結果。

## 一起讓中文化更好

- **翻譯有問題**：遊戲中按 **Ctrl+Shift+T**，中文化會記下畫面上的翻譯。離開遊戲後，首頁會幫你整理成回報檔，裡面列出每一句的編號與譯文。
- **閃退**：首頁會詢問是否建立回報檔。閃退的原因還在調查，每一份回報都很有幫助。
- **其他問題**：按首頁的「回報問題」。

回報檔只有中文化的記錄、設定和系統版本，不含個人資料。請到 [Issues](https://github.com/pmanyeh/Ultima-9-for-zh/issues/new/choose) 選擇「翻譯問題」或「閃退或錯誤」，把回報檔拖進說明欄。

目前已知的問題列在 [`說明.txt`](release/說明.txt) 的「已知的問題」一節。

## 運作方式

- **不散布遊戲檔案**：發布包只有中文化程式、中文譯文與「差異檔」。第一次開始遊戲時，首頁從玩家自己的遊戲檔產生中文資料，放在中文化資料夾裡。路標用的是我們自己畫的文字圖層，疊在玩家遊戲裡的空白木牌上。
- **不永久修改遊戲**：開始遊戲時才換上中文文字檔，離開時換回，並以雜湊檢查每一個檔案。路標的貼圖從中文化資料夾讀取，完全不寫入遊戲資料夾。
- **顯示中文**：原版只能顯示英文字母。中文化在遊戲執行期間載入一個小程式（以 `dsound.dll` 的身分放在遊戲資料夾，離開時移除），攔下遊戲的文字繪製，改用中文字型畫出。

## 給開發者

主要目錄：

| 目錄 | 內容 |
| --- | --- |
| `runtime/` | C++（MSVC，Win32）：中文化首頁、啟動器、執行期 DLL、影片播放器與測試 |
| `u9cht/`、`scripts/` | Python：FLX 讀寫、翻譯資料建置、打包 |
| `translation/` | 譯文主檔（`zh-TW/*.jsonl`）、譯名表、翻譯規範、路標文字圖層 |
| `docs/` | 格式與逆向研究（`formats/`、`research/`）、發布流程（`release/`） |
| `reports/` | 各開發階段的報告 |

建置與測試（需要 Visual Studio 2022 以上、Python 3.12 以上）：

```powershell
cmake -S runtime -B build -A Win32
cmake --build build --config Release
ctest --test-dir build -C Release
python -m unittest discover -s tests
```

建置中文化資料與打包需要你自己的遊戲檔，見 [發布流程](docs/release/RELEASE_PROCESS.md)。翻譯的規則見 [翻譯規範](translation/TRANSLATION_GUIDE.md)。

參與開發前請先讀 [AGENTS.md](AGENTS.md)，最重要的規則是：**絕對不要提交任何遊戲檔案、從遊戲抽出的英文原文，或含本機路徑的設定**。

## 致謝

- **范綱維**：在 Mac 上實機測試中文化，找出並修正了 macOS 版 Wine 上影片黑畫面、舊版 Wine 無法開始遊戲的問題（[#1](https://github.com/pmanyeh/Ultima-9-for-zh/pull/1)），也整理了 Apple 晶片上各版本 Wine 的穩定度，讓 Mac 玩家也能玩到中文版。
- [tgwUltima／TITAN](https://github.com/theGreyWanderer-uc/tgwUltima)（theGreyWanderer）：FLX 格式的研究與讀取程式（MIT 授權）。同作者的 [tgwUltima9GL](https://github.com/theGreyWanderer-uc/tgwUltima9GL) 是遊戲程式位址、原版問題與貼圖格式的參考（只參考，未使用其程式碼）。
- 字型：[jf open 粉圓](https://github.com/justfont/open-huninn-font)（justfont），路標使用 Noto Sans TC（Google），皆為 SIL Open Font License 1.1。
- 所有回報問題、幫忙校稿的玩家。

## 授權

本專案的程式碼、譯文與文件以 [MIT 授權](LICENSE)釋出。第三方元件（內附的 TITAN 讀取程式、字型等）依各自的授權，完整說明見 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。遊戲本身與其內容的權利屬於 Electronic Arts，不在本授權範圍內。

## 聲明

本專案是玩家自製的非官方中文化，與 Electronic Arts、Origin Systems、GOG、Valve 無關。Ultima 為 Electronic Arts 的商標。請使用正版遊戲。
