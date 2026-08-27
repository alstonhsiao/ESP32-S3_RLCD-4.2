# Web 用量分析

`aiusage-analysis.html` 是先於 RLCD 驗證資料語義的電腦端原型。目前仍為 Claude、Codex、Grok、Ollama 四張分圖（舊 `codex` key），並固定只顯示最近 10 天，避免來源疊線或歷史過長後難以判讀。韌體與 `ui/` wireframe 已改五源（`codex:chihyi`、`codex:Alston`）；本原型尚未同步。

## 開啟

在 repo 根目錄執行：

```bash
python3 web/serve.py
```

再開啟：

```text
http://127.0.0.1:8000/aiusage-analysis.html
```

伺服器會把 `/data` 代理到目前的 aiusage API。若無法連線，也可直接在頁面匯入 `/data` JSON 快照。

## 圖表判讀

- 實線是剩餘額度 `100 - used_*_pct`；向下代表消耗增加，reset 時會向上跳回。
- 灰色虛線是從目前週期錨點到下一次 reset 的理想剩餘下降斜率。
- 若下一次 reset 提前發生，理想線會在該 reset 時間依原 due/reset 斜率截斷，不會錯誤延伸到 0%；下一週期再從 100% 重新開始。
- `R` 是數值跳變且符合 reset 證據；`?` 是只有數值跳變的疑似事件；`next` 是 API 回報的下一次 reset。
- 圖表與事件表固定只取每個來源最新快照往回 10 天的資料；理想線可延伸到新的 due/reset 時間，右側淡色區域代表未來投影，不是歷史資料。
- reset 後若未累積至少 6 小時且 3 個有效點，實測斜率顯示 `--`；目標斜率仍依新 reset 到 due/reset 計算。
- FAST／OK／SLOW 仍以內部 `used_*_pct` 計算消耗斜率，再與理想斜率比較；容許值可在頁面調整，預設為 ±10%。
