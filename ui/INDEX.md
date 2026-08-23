# ui 路由索引

## 總覽

| 檔案 | 內容形態 | Agent 何時需要 | 下一步 |
| --- | --- | --- | --- |
| `aiusage-wireframe.html` | 現行 P0–P5 單色頁面、Error／Partial 狀態與互動預覽 | 修改頁面資訊架構、版面、狀態或 wireframe 驗收時 | 先按頁面名稱定位，再與主 sketch 實作互相核對 |
| `../web/aiusage-analysis.html` | 電腦端四源圖表原型，供 RLCD P2–P5 的資料與斜率語義驗收 | 修改來源圖表、reset/due 或最近 10 天視窗時 | 先確認 Web 原型，再核對 `firmware/aiusage_home/aiusage_home.ino` |
| `README.md` | 人類預覽方式與單色設計約束 | 新增 UI 資產或確認預覽入口時 | 先讀設計約束與頁面表 |

## 路由摘要

| 項目 | 一句話說明 | 觸發條件 | 關鍵輸入／輸出 | ⚠️ 注意事項 |
| --- | --- | --- | --- | --- |
| `aiusage-wireframe.html` | 將 400×300 單色 RLCD 的 P0–P5 與失敗狀態做成可在瀏覽器預覽的版面稿。 | 修改 P0 Combined、P1 Trend、P2–P5 Source Trend 或 Error／Partial 狀態。 | 輸入頁面語義與 1bpp 約束；輸出人類可點選的 HTML wireframe。 | 這是設計預覽，不是裝置執行環境；P2–P5 的資料與斜率語義另以 `../web/aiusage-analysis.html` 驗證。 |
| `README.md` | 說明如何開啟 wireframe 以及目前的單色畫布規則。 | 新增或搬移 UI 檔案，或需要更新預覽指引時。 | 輸入 UI 目錄結構；輸出人類操作說明。 | 不要把 README 當成韌體行為的唯一來源；資料語義見 `../docs/ui-and-data.md`。 |

## 頁面定位

- P0 Combined：搜尋 `P0 · Combined`，確認週剩餘、5h、day%、reset 與 pace 的 2×2 版面。
- P1 Trend：搜尋 `P1 · Multi-source Trend`，確認最近 10 天、四種線型與 100/7 輔助線。
- P2–P5：搜尋各來源 `Source Trend`，並以 `../web/aiusage-analysis.html` 驗收 reset/due 與提前 reset 截斷斜率。
- Error／Partial：搜尋 `Error / Partial states`，確認空資料重試失敗與部分來源失敗仍可讀。
