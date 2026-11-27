# NEED_REVIEW 歸檔

依 `AGENTS.md`「🔵 只准追加」規則：已解決的 `NEED_REVIEW` 經使用者核准後搬移至此，原文保留不刪。

## 2026-11-27 歸檔（使用者核准）

分類：場景過時（wireframe／ALL DETAIL SVG 與韌體不同步，已於 2026-11-26、2026-11-27 同步完成）。

### 1. `docs/power-and-progress.md`（2026-11-27 頁序調整段）原文

- R1 400×300、1bpp、刷新與輪詢策略不變；實機燒錄後開機輪詢正常。`ui/aiusage-wireframe.html` 尚未同步新頁序（NEED_REVIEW）。
- wireframe 同步（同日）：上方 NEED_REVIEW 已處理。`ui/aiusage-wireframe.html` 依新頁序重排 8 個 panel（頁碼、標籤、toolbar、說明同步；互動延遲更新改寫為 5 秒），`ui/all-detail-d.svg`／`all-detail-d-partial.svg` 以像素層級交換第 2、3 欄內容（x=73–135 ↔ 137–199，分隔線不動），保留 2026-11-26 快照數值；Chrome headless 截圖確認版面正常。建議歸檔上方 NEED_REVIEW 標記。

### 2. `docs/ui-and-data.md` 原文

`ui/aiusage-wireframe.html` 與 `ui/all-detail-d*.svg` 仍為五欄稿：NEED_REVIEW（待依實機畫面同步）。
  → 2026-11-26 已同步：以 host 端 U8g2（實際字型）執行 `renderCombined()` 重新輸出六欄 SVG（正常／partial），三種情境（正常、partial、最寬數值）文字距欄界皆 ≥2px；wireframe 新增 P7 AGY、P1 第六線，頁碼改 /8。建議歸檔此 NEED_REVIEW。

### 3. `ui/INDEX.md` 總覽表 `all-detail-d.svg` 列原文

| `all-detail-d.svg`、`all-detail-d-partial.svg` | D 版面上方 258px 的 U8g2 字型輸出（正常／partial）；仍為五欄稿，韌體 2026-11-26 已改六欄 NEED_REVIEW → 同日已重新輸出六欄快照，建議歸檔此標記；2026-11-27 依新欄序 Claude／Alston／Chihyi／… 交換第 2、3 欄像素 | 檢查 P1 All Detail 目前的字型、分欄與異常狀態 | 由 `aiusage-wireframe.html` 載入，固定快照供視覺驗收 |
