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


### 2026-10-16：ALL DETAIL gap 與週預測

- 使用者授權：ALL DETAIL 改用網站 gap 節奏、保留每日預算，新增本週累積平均消耗的提前用完／到期剩餘與 USD 估值；指定 Grok 4.7 實作、主 agent 審核。實際以本機 Grok CLI `grok-4.7` 委派；Grok 交付公式 header、host 測試與韌體，主 agent 完成文件／wireframe、審核修正與上板。
- P0 節奏為剩餘相對七天理想線的 gap：±3pp 內 ON PACE，超過 ±3 至 ±10pp 為 SLOW／FAST，嚴格超過 ±10pp 為 VERY SLOW／VERY FAST；P2–P6 保留既有斜率語義。D 仍是剩餘除以剩餘天數，預測則用 `(100−R)/(7−d)`，以快照時間計算，不把預算當消耗速度。
- 預測／USD 計算集中於 `firmware/aiusage_home/usage_forecast.h`，含剛重置、過期、未來／缺失快照、無時鐘、失敗來源與零消耗等退化狀態；預設月費與四週制對齊網站。主 agent 修正 exact-zero 浮點誤差，避免剛好用完被錯判成提早用完；保留粗體名稱與節奏，五列每源新增第三行。
- 驗證：89 項 C++ host 檢查通過；192 組 C++ 與網站 JavaScript 數值／狀態對照無差異；網站既有 21 項分析／預測測試通過。使用實際 U8g2 字型與 P0 繪製程式驗證正常／partial／最長欄位，均在 R1 400×300 1bpp 畫布內。
- 鎖定 `flash-firmware` 腳本編譯／上傳成功：程式 1,216,379 bytes（38%）、global RAM 115,656 bytes（35%）。腳本 Serial 25 秒檢查回傳 code 4，另以完整開機監看補驗通過：`poll ok: pts=4444 trend=364 C=3 H=75 A=66 G=96 O=66`，五源 `P0` 診斷皆有輸出、`setup done` 與 `WiFi radio off` 正常。Serial 開啟造成一次 USB_UART_CHIP_RESET，受控重新監看後確認恢復。
- 09:10:23 快照（epoch 1792113023）：CLAUDE ON PACE／LEFT 1% ~$0.05；CHIHYI VERY FAST／EARLY 4.0D；ALSTON VERY FAST／EARLY 4.8D；GROK ON PACE／WARMUP；OLLAMA VERY SLOW／LEFT 40% ~$2.00。快照已更新，與最初 08:40 的 chihyi FAST 不同是正常資料變化。
- Wi-Fi、15 分鐘輪詢、分鐘級刷新、5 分鐘翻頁與 10 秒互動延遲更新維持原設定；未新增連線或提高刷新頻率。空資料 trigger/retry 控制流程保留，未對雲端資料做修改或觸發測試。沒有 commit／push。
- 本機環境修復：Arduino 的 Intel `ctags` 在 arm64 無法啟動，已由同 tag 原始碼重建 native，保留原檔備份；來源、兼容巨集修正與復原位置見 `docs/firmware-operations.md`。

#### 網站計算檢視（建議，尚未修改網站）

| 優先序 | 已觀察的問題／限制 | 建議 |
| --- | --- | --- |
| 高 | `calculatePace`／每日預算使用現在時間，`forecastWeekly` 使用快照時間；重新計算同一舊快照時，pace 可從 FAST 改善成 ON PACE，但預測不變。重現：剩46%、距重置3.5天，六小時後重新計算 pace 從 FAST 變 ON PACE，預測仍提前0.5天用完。 | 節奏與預測使用一致快照時點；倒數另外用現在時間。 |
| 高 | 預測有六小時失效保護，但候選推薦未套用快照新鮮度；頁面開著時目前定時器只更新資料年齡，沒有同步重算／隱藏失效預測與推薦。 | 統一 freshness gate；資料變舊或 reset 到期時，定時取消數字預測與推薦。 |
| 高 | 「今日可用」其實是每日平均預算：剩80%、半天後重置會算出160%。 | 改名「平均每日預算」；若要表達本週剩餘額度中接下來最多24h可用，需另算並最多為當前剩餘。 |
| 中 | 月費按供應商硬編碼，多個 Codex 帳號只能共用同一價格；月費/4 是四週制估算。 | 每帳號設定實際月費與週分攤方式，清楚標為未使用價值估算、非退款；另應統一金額最後一位的四捨五入規則（C printf 與 JS toFixed 在半分邊界可能差 USD 0.01，例如 2.625）。 |
| 中 | 本週累積平均對最近突然大量使用或停用反應較慢。 | 保留週平均主預測，可選近24h速度作輔助，需排除跨 reset 與不足樣本，勿不加說明混用。 |
