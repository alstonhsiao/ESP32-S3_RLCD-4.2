# UI — 單色設計稿

本目錄放 **RLCD 400×300 / 1bpp** 的 wireframe 與版面說明。  
先設計、再寫韌體（Arduino + U8g2）。

## 檔案

| 檔案 | 說明 |
| --- | --- |
| [aiusage-wireframe.html](./aiusage-wireframe.html) | AI 週剩餘用量儀表（對齊 aiusage-web） |
| [../web/aiusage-analysis.html](../web/aiusage-analysis.html) | 電腦端四源分析圖表；P4–P7 RLCD 頁面的資料、reset/due 與斜率原型 |

## 如何預覽

用瀏覽器開啟 HTML 即可，無需伺服器：

```bash
open ui/aiusage-wireframe.html
```

或在 Finder 中雙擊該檔。

## 設計約束（與硬體一致）

- 畫布 **400×300** 橫向（U8g2 `U8G2_R1`）
- 僅黑 / 白；層次靠反白、線框、字級
- 參考 [ClaudeSlate](https://github.com/HarryXin0919/ClaudeSlate) 的資訊密度與底欄
- 資料語意：週剩餘 % = `100 − used_weekly_pct`

## 頁面規劃（aiusage）

| 頁 | 內容 |
| --- | --- |
| P0 Home | 時鐘 + Claude/Codex/Grok/Ollama 週剩餘 % + bar |
| P1 Detail | 表格式：week / 5h / reset；低剩餘反白警示 |
| P2 Trend | 多源剩餘折線（線型區分）+ 100/7 輔助虛線 |
| P4–P7 Source Trend | Claude、Codex、Grok、Ollama 各自一頁；最近 10 天剩餘曲線、reset/due 標記與提前 reset 截斷的理想斜率（電腦端原型見 `../web/aiusage-analysis.html`） |
| States | no data / partial / offline |

BOOT 短按翻頁（之後韌體實作）。
