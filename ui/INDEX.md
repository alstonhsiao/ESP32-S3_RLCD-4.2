# ui 路由索引

## 總覽

| 檔案 | 內容形態 | Agent 何時需要 | 下一步 |
| --- | --- | --- | --- |
| `aiusage-wireframe.html` | 大型單檔 HTML，內含 CSS、頁面 mock 與互動腳本 | 修改頁面資訊架構、版面、狀態或 wireframe 驗收時 | 先按頁面名稱定位 |
| `README.md` | 人類預覽方式與單色設計約束 | 新增 UI 資產或確認預覽入口時 | 先讀設計約束與頁面表 |

## 路由摘要

| 項目 | 一句話說明 | 觸發條件 | 關鍵輸入／輸出 | ⚠️ 注意事項 |
| --- | --- | --- | --- | --- |
| `aiusage-wireframe.html` | 將 400×300 單色 RLCD 的 P0–P4 狀態做成可在瀏覽器預覽的版面稿。 | 修改 P0 Home、P1 Detail、P2 Trend、P3 Pace、P3b 或 Error／Partial 狀態。 | 輸入頁面語義與 1bpp 約束；輸出人類可點選的 HTML wireframe。 | 這是設計預覽，不是裝置執行環境；改版後要與主 sketch 互相核對。 |
| `README.md` | 說明如何開啟 wireframe 以及目前的單色畫布規則。 | 新增或搬移 UI 檔案，或需要更新預覽指引時。 | 輸入 UI 目錄結構；輸出人類操作說明。 | 不要把 README 當成韌體行為的唯一來源；資料語義見 `../docs/ui-and-data.md`。 |

## 頁面定位

- P0：搜尋 `P0 Home`，確認時鐘、四源週剩餘、bar 與底欄。
- P1：搜尋 `P1 Detail`，確認 WEEK／5H／RESET 與低剩餘反白。
- P2：搜尋 `P2 Trend`，確認有限歷史點、線型區分與輔助線。
- P3／P3b：搜尋 `Daily Pace`，比較敘事版與表格版方案。
- P4：搜尋 `Empty / Error`，確認無資料與部分來源失敗仍可讀。
