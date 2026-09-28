# 功耗與進度

本模組收納電池觀測、長測計畫、歷史決策、目前待辦與刻意不做的範圍。電量來自 GPIO4 ADC 百分比，只適合粗估，不能取代庫侖計。

## 18650 續航觀測

### 短區間觀測

2026-08-05 夜間至 2026-08-06 早晨，always-on aiusage 儀表由 87% 降至 77%，約九小時內耗用 10%。依該區間粗估，滿電約可用三至四天；這只是短區間推算。

### 追加觀測

2026-08-06 早晨至 2026-08-07 晚間，由 77% 降至 19%，約一天半內耗用 58%。此區間推算的滿電續航約兩至三天，與前一段不同，暫不能視為校準結果。

### 長測計畫

- [ ] 從滿電或已知高電量跑到低電／關機，記錄時間戳與電量。
- [ ] 標註 Wi-Fi 是否常連、輪詢間隔、互動與語音狀態。
- [ ] 至少完成一個完整放電週期，最好再做一次對照。
- [ ] 將結果補回本檔；若與既有推算差異大，再查輪詢節奏與刷新策略。

ADC 中段曲線較平，日間使用可能更耗電，低電末段也可能掉得更快；不要用單次觀測承諾固定續航。

## 進度與待辦

| 領域 | 現況 |
| --- | --- |
| 基建 | SPEC、Arduino + U8g2、Hello RLCD 與 aiusage 多頁已完成 |
| aiusage | 現行程式為 P0 Combined、P1 Trend、P2–P6 五源最近 10 天圖表；2026-08-26 從 4 源擴充為 5 源（新增 codex:Alston），P0 改五列橫排；待實機確認 |
| UX | HTML wireframe 已同步現行 P0–P6；P0 Combined 五列間距、P1 五線可讀性與軟體反顯仍待實機確認 |
| 穩定度 | HTTPS／較大 JSON、離線 stale、重試與精簡 API 待驗證 |
| 感測與互動 | SHTC3、KEY 第二操作、天氣或本機 proxy 待規劃 |
| 工程 | `aiusage_home` 拆分 ui／net／data、GitHub remote 檢查可選 |

## 歷史決策

| 日期 | 決策或成果 |
| --- | --- |
| 2026-08-04 | 建立 SPEC 基建、ClaudeSlate 參考、鎖定 Arduino + U8g2；Hello 燒錄成功 |
| 2026-08-04 | aiusage P0／P1／P2 與 BOOT 翻頁完成並確認 |
| 2026-08-05 | P3 Daily Pace 採表格式方案，wireframe 補上 P3／P3b |
| 2026-08-06 | 完成第一段電池短測，長測列為待辦；整理 handoff 內容回本 repo |
| 2026-08-07 | 完成追加電池觀測；仍以完整放電週期為準 |
| 2026-08-11 | Web 四源圖表移植至 RLCD P4–P7：各頁顯示最近 10 天剩餘曲線、reset/due 與 reset-aware 理想斜率；已通過本機編譯，待燒錄驗證 |
| 2026-08-22 | 移除舊 P0 Home、P1 Detail、P3 Pace，將合併頁升為 P0；現行頁面重編為 P0–P5，文件與 wireframe 同步列入清理 |
| 2026-08-23 | 完成專案清理：同步 P0–P5 wireframe、移除退役 panels／死碼／重複燒錄說明，並合併 UPD 與頁面 full-buffer 傳送；本機編譯與桌面／手機視覺檢查通過，待實機驗證 |

## 刻意未做

- 不在螢幕上開瀏覽器或渲染彩色網頁。
- 不把 ESP-IDF 或 ESPHome 放進主線。
- 暫不做語音／喇叭功能，除非另有高耗電模式說明。
- 不提交 `../firmware/**/secrets.h`。

## 歷史決策紀錄（自 AGENTS.md 移入）

### 已解決事故（2026-08-16）

- **螢幕反覆沒資料**（同類發生多次 → 升格）：舊版雲端 `usage-web` 用檔案系統 JSON 快取（`history.json`），Zeabur 重新部署時清空 → `GET /data` 返回 `{"points":[]}` → 韌體直接放棄顯示，且所有失敗路徑無 Serial 輸出，無法診斷。修復：(1) 雲端改用 SQLite + persistent volume（keyboardmaestro 專案 `autousage/usage-web/db.js`）；(2) 韌體在 0 points 時自動 `POST /trigger` 觸發 KM 查詢 + sync 再重試；(3) 所有失敗路徑加 Serial 診斷。反例：不要假設雲端 `/data` 永遠有資料；韌體必須能處理空回應並自動恢復。

### 已解決事項（2026-08-25）

- **P2–P5 理想虛線各頁陡度不一**（單次設計缺陷，未升格）：`TREND_N=128` 在當時約 23 分鐘一點的取樣下只涵蓋最近約 2 天，視窗外的週期 reset 使 `drawTrendIdeal` 退化成「自窗起點剩餘值清零到 due」的半高平緩線；reset 恰在窗內的來源卻畫滿高 100%→due 線，兩種語義在同一組頁面混用且無標示（例：GROK 頁虛線特別平）。修復：`TREND_N` 擴為 720（10 天 × 20 分鐘格），解析時對較密雲端點逐格保留最後一點做降取樣（現由 `parseStream` 串流掃描實作）；reset 事件陣列另立 `TREND_EVENT_MAX=32` 不隨 `TREND_N` 放大；P1 多線繪製改串流逐段畫，避免 720 點座標陣列吃堆疊。實機 Serial 驗證 `poll ok … trend=270`，四源皆取得真實 R 錨點。反例：不要讓趨勢緩衝實際涵蓋遠小於標示（LAST 10D）的時間窗；依賴歷史錨點的功能會靜默退化。

### 已解決事項（2026-08-26）

- **Codex 多帳號：`codex` key 拆分為 `codex:chihyi` + `codex:Alston`，韌體從 4 源擴充為 5 源**（功能擴充，非事故）：usage-web `/data` 不再返回 `codex`，改返回 `codex:chihyi`（原帳號 chihyi.a@gmail.com）與 `codex:Alston`（新增 alstonh@gmail.com）。韌體硬編碼的 `const char* keys[4]` 會找不到 `codex` 而靜默顯示 missing。修復：新增 `N_SOURCES=5` 常數取代所有寫死的 4；來源鍵改為 `SRC_KEYS[N_SOURCES]` = `{"claude","codex:chihyi","codex:Alston","grok","ollama"}`；`SourceUi src[N_SOURCES]` 新增 CHIHYI/ALSTON（顯示名稱區分兩個 Codex 帳號）；`PAGE_COUNT=7`（P0+P1+5 源頁）；P0 從 2×2 grid 改為五列橫排（每列 44px，名稱+origin 在上、大字 remain%+bar+5h/day/week-reset 在下）；P1 新增第 5 種線型（style 4 = thin dashed）與 legend 條目；Serial log 改為 `C/H/A/G/O`。編譯通過無新 warning。待實機驗收 P0 五列間距與 P1 五線可讀性。

### 已解決事項（2026-09-01）

- **節奏標籤五級化 + 英文化**（功能對齊，非事故）：上游 keyboardMaestro 的 usage-web 已改五級英文標籤（gap = 週剩餘% − 7 天理想進度剩餘%，±3pp 內 ON PACE；±3~±10pp SLOW/FAST；超過 ±10pp VERY SLOW/VERY FAST）。本專案三套 pace 實作（P0 日額對比 100/7、P2–P6 斜率對比、web 原型）與 wireframe 靜態字、五份文件一併對齊，但保留各自既有判斷式（未改成 gap 公式，因本專案裝置語義是「消耗斜率 vs 理想斜率」與「建議日額 vs 100/7」，非 usage-web 的剩餘 gap）。最終值：標籤 `VERY SLOW / SLOW / ON PACE / FAST / VERY FAST`（P0 內圈 12/18 維持、外圈 7/22；斜率式內圈 ±10% 維持、外圈 ±20%，web 原型外圈 = 內圈輸入值×2）；`RESET`、`--`、`N/A` 退化狀態保留；web 原型補 `.status-label.very-slow/.very-fast` CSS（深化色 `--warning-strong`/`--danger-strong`）。韌體經實機燒錄 + Serial 驗證；web 原型以 Playwright 對 tolerance 輸入做 0–60% 掃描，標籤翻轉點符合「外圈=內圈×2、嚴格不等式」。反例：wireframe 的範例標籤要跟著公式重算，不能只換字（P5 GROK 18.1 vs 14.3 = 1.27×，落在 ±20% 外圈，正確標籤是 VERY FAST 而非 FAST）。
