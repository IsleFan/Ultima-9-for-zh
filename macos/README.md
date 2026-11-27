# macOS 安裝包

把原作者發佈的中文化、Wine 執行環境與安裝程式打包成 `U9CHT-<版本>-macOS.zip`，讓 Mac 玩家雙擊就能安裝、遊玩。
中文化本身不在這裡修改；這個資料夾只有 Mac 端的安裝與啟動腳本、說明與輔助工具。

## 內容

| 路徑 | 說明 |
| --- | --- |
| `build_package.sh` | 打包腳本 |
| `package/1_安裝.command` | 安裝程式：檢查 macOS 14+／Rosetta、解除隔離、遊戲來源二選一（GOG 離線安裝檔以 innoextract 解開，或已安裝的遊戲資料夾）、下載並驗證 Wine 引擎、建立 Wine 環境 |
| `package/2_啟動 U9 中文版.command` | 啟動：多螢幕時選擇螢幕（暫設為主要顯示器，離開遊戲後還原）、依螢幕高度調整首頁 DPI |
| `package/安裝與使用說明.html` | 給玩家的安裝與使用說明 |
| `package/runtime/tools/env.sh` | Wine 環境變數與 DLL 覆寫（`ddraw,dsound=n,b`） |
| `package/runtime/licenses/README.txt` | 第三方元件與授權 |
| `src/u9display.swift` | 列出螢幕、切換主要顯示器、還原（打包時編譯成 universal binary） |

範本裡的 `@VERSION@` 在打包時換成版本號。

## 打包

```sh
macos/build_package.sh --cht path/to/U9CHT-1.0.0.zip
```

- `--cht`：原作者發佈的中文化 zip 或解開的資料夾，原樣放進 `runtime/U9CHT`。版本從名稱取（`U9CHT-1.0.0` → `1.0.0`），或用 `--version` 指定。
- 輸出在 `.local/macos-build/`（已被 `.gitignore` 排除）：`U9CHT-<版本>-macOS/`、`.zip`、`.zip.sha256`。
- 需要 macOS 與 Xcode Command Line Tools（`swiftc`、`lipo`）。

打包時下載並比對 SHA-256 的第三方檔案（快取在 `.local/macos-build/cache/`）：

| 檔案 | 用途 | 來源 |
| --- | --- | --- |
| `innoextract-1.9-windows.zip` | 在 Wine 內解開 GOG 離線安裝檔（GOG 自己的安裝程式在 Wine 下會 stack overflow） | [dscharrer/innoextract 1.9](https://github.com/dscharrer/innoextract/releases/tag/1.9) |
| `wine-staging-11.21-osx64.tar.xz` | 只取其中的 dylib（FreeType、MoltenVK、GStreamer 等），給 Sikarugir 引擎用 | [Gcenx/macOS_Wine_builds 11.21](https://github.com/Gcenx/macOS_Wine_builds/releases/tag/11.21) |

Wine 引擎本身不打包，由 `1_安裝.command` 在玩家電腦上下載並驗證：
`WS12WineSikarugir10.0_8.tar.xz`，SHA-256 `2a6bcf2a3bf13cddf825599b6bc1e3659cbf71e1282c6d77141a92f6ac8d4c68`
（[Sikarugir-App/Engines v1.0](https://github.com/Sikarugir-App/Engines/releases/tag/v1.0)）。

## 為什麼用 Sikarugir Wine 10.0

在 macOS 27、Apple Silicon 上實測（細節見 `docs/research/U9_WINE_MAC.md`）：

- WineHQ 官方 Wine 10／11 執行 32 位元程式會隨機當機（WineHQ bug 60494，Rosetta 相關，官方不修），原版遊戲也一樣。
- Sikarugir Wine 10.0 rev 8 沒有出現這個當機，中文化首頁、遊戲與影片字幕都正常。
- Sikarugir Wine 11.0 在 macOS 27 上無法建立 Wine 環境；CrossOver 24 原始碼版缺 `CopyFile2`（中文化 1.0.0 已不需要）。

## 已知限制

- 需要 macOS 14 以上（Sikarugir Wine 10.0 的最低版本）；Intel Mac 尚未實測。
- 首頁的「視窗化」在 Mac 上沒有作用（不當機，但仍佔滿螢幕）。
- 安裝時需要網路（下載約 166 MB 的 Wine 引擎）；已下載過的引擎檔放在安裝包資料夾或「下載項目」可免下載。
