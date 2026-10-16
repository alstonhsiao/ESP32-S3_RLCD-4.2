# UI — 單色設計稿

本目錄放 **RLCD 400×300 / 1bpp** 的 wireframe 與版面說明。  
先設計、再寫韌體（Arduino + U8g2）。

## 檔案

| 檔案 | 說明 |
| --- | --- |
| [aiusage-wireframe.html](./aiusage-wireframe.html) | 現行 P0–P6 與 Error／Partial 狀態的 AI 週剩餘 wireframe |
| [all-detail-layout-options.html](./all-detail-layout-options.html) | ALL DETAIL 五種排版比較：D 五欄儀表已選用，其餘保留作比較 |
| [all-detail-d.svg](./all-detail-d.svg)、[all-detail-d-partial.svg](./all-detail-d-partial.svg) | 現行 D 方案上方 258px 的 U8g2 字型輸出（正常／部分失敗），由主 wireframe 載入 |
| [../web/aiusage-analysis.html](../web/aiusage-analysis.html) | 電腦端四源分析圖表（仍用舊 `codex` key，尚未同步韌體五源）；P2–P6 的 reset/due 與斜率原型 |

## 如何預覽

用瀏覽器開啟 HTML 即可，無需伺服器：

```bash
open ui/aiusage-wireframe.html
```

或在 Finder 中雙擊該檔。

比較新的 ALL DETAIL 排版：

```bash
open ui/all-detail-layout-options.html
```

五款使用同一份固定快照，可切換極端數值、部分失敗、快照過期，並選擇原生 400×300 或單款放大。比較稿的瀏覽器字型為近似。現行 D 版面與部分失敗狀態已使用實際 U8g2 字型輸出同步到 `aiusage-wireframe.html`；資料拉取、分鐘刷新、Wi-Fi 與電池策略保持原設定。

## 設計約束（與硬體一致）

- 畫布 **400×300** 橫向（U8g2 `U8G2_R1`）
- 僅黑 / 白；層次靠反白、線框、字級
- 參考 [ClaudeSlate](https://github.com/HarryXin0919/ClaudeSlate) 的資訊密度與底欄
- 資料語意：週剩餘 % = `100 − used_weekly_pct`

## 頁面規劃（aiusage）

| 頁 | 內容 |
| --- | --- |
| P0 Combined | 週剩餘 + bar、5h、建議一天額度、reset、gap 五級節奏、提前用完／到期剩餘與 USD 估值的 D 方案五欄儀表 |
| P1 Trend | 多源剩餘折線（五種線型區分）+ 100/7 輔助虛線 |
| P2–P6 Source Trend | Claude、Chihyi、Alston、Grok、Ollama 各自一頁；最近 10 天剩餘曲線、reset/due 標記與提前 reset 截斷的理想斜率（電腦端原型見 `../web/aiusage-analysis.html`） |
| States | no data / partial / offline |

BOOT 短按翻頁已在韌體實作；停止操作約 10 秒後會拉取新資料。
