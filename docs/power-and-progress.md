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
| aiusage | 現行程式為 P0 Combined、P1 Trend、P2–P7 六源最近 10 天圖表（2026-11-26 新增 agy）；2026-08-26 從 4 源擴充為 5 源（新增 codex:Alston），P0 改五列橫排；待實機確認 |
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

### 2026-10-16：ALL DETAIL 選用 D 五欄儀表

- 使用者選定五款模擬中的 D 方案。P0 由五個橫列改為五欄，每欄包含名稱、週剩餘大字／bar、兩行節奏、兩行預測（LEFT 與 USD 或 EARLY 與天數）、5H／D／W。省略 origin，保留最高每日平均預算的 `*`，標頭改顯示本地時間並避開 UPD；底欄與其他頁面維持既有行為。
- 維持 U8G2_R1、400×300、ST7305 1bpp、固定 SPI 腳位與環境光閱讀假設。預測／gap 公式、15 分鐘輪詢、分鐘重畫、5 分鐘翻頁、Wi-Fi radio-off 與空資料 trigger/retry 均未改動，沒有提高連線或刷新頻率。
- 實際 U8g2 字型與 P0 程式檢查九種情境（正常、部分失敗、最寬數值、過期、剛重置、額度用完、reset 到期、無效資料與 UPD 預留）：文字無跨欄或重疊；89 項計算檢查通過。正常／partial 的 1bpp 輸出轉為 `ui/all-detail-d.svg` 與 `ui/all-detail-d-partial.svg`，瀏覽器載入後逐像素一致；現行 wireframe 與 README／INDEX 同步。五款比較稿保留並標示 D 已選用。
- 鎖定 flash-firmware 腳本編譯／上傳、hash 驗證成功，程式 1,225,019 bytes（38%），global RAM 115,656 bytes（35%）。腳本因 25 秒內未等到完整開機字串回 code 4，當時已收到 HTTP 200；接續監看收到 CHIHYI／ALSTON／GROK 預測、WiFi radio off 與 setup done，確認資料已解析、預測與啟動完成。
- 補驗限制：接續 raw Serial 監聽漏掉 poll ok 前段，但已收到預測與 setup done；再以 pyserial 開啟監聽 110 秒未產生新的開機輸出（未主動觸發重置），因此未取得一份完整的新開機紀錄。沒有繼續重試；此次上傳 hash 與已捕捉的啟動完成紀錄仍可確認燒錄／資料啟動成功。

### 2026-10-22：資料源遷移至 Contabo

- 原因：usage-web 已從 Zeabur 搬至 Contabo，舊站回 502，造成 RLCD 無法更新。依使用者選定的方案 A，韌體 `DATA_URL`／`TRIGGER_URL` 改用 `https://aiusage.apps.qianyinexus.com`，保留 `/data`／`trigger` 路徑；同步主 README、韌體 README、資料語義文件、Web 本機代理及 HTML 直接下載備援網址。
- 已知且接受的限制：新站 `GET /data` 公開可用，`POST /trigger` 需要 Basic Auth；韌體刻意不帶帳密，空資料時仍嘗試 trigger，預期 401 並只記 Serial，再重試一次 `/data`。因此空資料自動恢復目前只剩重試、等待 Mac 排程同步補資料，無法主動觸發上游查詢；本次正常非空輪詢未呼叫 trigger，也未強制製造空資料測試。
- 僅修改網址與註解，未改解析、15 分鐘輪詢、分鐘級刷新、5 分鐘翻頁、10 秒互動延遲更新或 Wi-Fi radio-off 策略；R1 400×300、1bpp 與環境光閱讀假設維持原狀。刷新與耗電策略未變，實際耗電未另行量測；約 3.5 MB 的 HTTPS 下載仍需較長連線時間。
- 驗證：依鎖定 `flash-firmware` 腳本在 `/dev/cu.usbmodem101` 編譯／上傳成功，程式 1,225,035 bytes（38%），global RAM 115,656 bytes（35%），上傳各區段均顯示 `Hash of data verified.`。腳本 25 秒 Serial 檢查未等到預期字串，exit code 4；已收到 `poll: HTTP 200 content-length=-1`，另接續監看並以一次 RTS reset 補取完整開機紀錄。
- Serial 補驗（接續監看完整 150 秒）：`poll: stream scanned 3499470 bytes, pts=4963 trend=397`、`poll ok: pts=4963 trend=397 C=38 H=51 A=85 G=2 O=100`，接著五源預測、`WiFi radio off` 與 `aiusage_home: setup done` 正常；未觀察到 HTTP 錯誤。排除 build、.git、AGENTS.md 與本檔歷史紀錄後，舊網域搜尋無結果；韌體去除註解後比對確認只變更 URL，`web/serve.py` 語法與 `git diff --check` 通過。未修改 AGENTS.md，沒有 commit／push。

### 2026-11-16：CHIHYI／ALSTON 相同數據調查（登入身分錯置，尚未修復）

- 雲端 `/data` 07:40:23（台北時間）仍為 CHIHYI 週已用 22%、ALSTON 9%；07:50:23 起兩者同為 9%，週重置時間也相同。15:50:23 快照兩者皆為已用 26%／剩餘 74%。重複資料已存在上游，韌體仍以不同 source key 讀取。
- 本機唯讀核對：採集設定仍將 chihyi 對應 `~/.codex`、Alston 對應 `~/.codex-alston`，但兩份 `auth.json` 的登入身分與 `account_id` 均屬 Alston。`~/.codex/auth.json` 修改時間為今日 07:46:19，與下一次採集開始重複的時間吻合；未輸出 token 或完整 account ID。
- 採集程式 `../keyboardMaestro/autousage/scripts/notify_all.sh` 依設定標籤命名來源，未驗證真實登入身分。`../keyboardMaestro/docs/agent-progress.md` 的 2026-11-11 條目記載同類事故；目前 `../ai-dev-env/tools/codex/codex-multi-account.zsh` 仍依 `~/.codex-active` 覆寫 `CODEX_HOME`，`codex-whoami` 只顯示選定標籤。此為可能重犯路徑，但本次沒有證據確認今日實際執行了哪條登入命令。
- 待修復：讓 chihyi 專用目錄重新登入正確帳號，繞過會覆寫目錄的 shell wrapper，登入後核對身分；採集端宜加入帳號身分驗證。07:50 起 CHIHYI 的錯置歷史須另行決定如何標記／處理，不可當成真實 chihyi 用量。
- 本次只調查並追加紀錄，未修改登入憑證、採集程式、本機／雲端歷史或韌體；資料拉取、空資料重試、刷新、Wi-Fi 與功耗策略均未變，未 commit／push。

### 2026-11-26：新增 AGY 來源（六欄）

- 上游 `/data` 已有 `agy`（Google AI Pro）。韌體新增第 6 來源、P0 改六個 64px 欄、P1 加細點線、P7 為 AGY 單源圖；月費估 20 USD。設計與座標見 `docs/ui-and-data.md`「AGY 來源與 ALL DETAIL 六欄」。
- 實機燒錄成功，Serial：`poll ok: ... O=98 Y=96`、`P0 AGY: remain=96.3 ... forecast=LEFT 95% ~$4.75`。
- 同日修正 `flash.sh` 誤報 exit 4：Serial 驗收窗 25→120 秒（5.4 MB `/data` 掃描約 30–60 秒以上才出 `poll ok:`；75 秒仍曾逾時，且埠重開時常截掉 boot 字串前段）。
- 不改資料拉取頻率、Wi-Fi 策略或刷新節奏；耗電不變。P0 欄名改 helvB08 以保留欄界留白；`ui/` wireframe 與六欄 SVG 快照已同步（host 端 U8g2 實際字型輸出）。2026-11-26 使用者實機目視確認六欄（含 helvB08 欄名）清楚可讀。

### 2026-11-27：輪詢改背景執行，按鍵不再被阻塞吞掉

- **動機**：`/data` 輪詢（Wi-Fi 連線 + 5.5 MB 串流掃描約 6–10 秒）原本同步跑在 `loop()` 內，期間 `handleButton()` 完全沒被輪詢，短按 BOOT 直接遺失，使用者看起來就是「當機」。同日先發生兩起同類誤判：使用者按了未綁功能的 KEY 鍵、以及按壓落在延遲輪該阻塞視窗，皆以 Serial 證據排除 MCU 當機。
- **改法**：輪詢移入一次性 FreeRTOS task（`startPollAsync`→`pollTaskFn`）；掃描器全部參數化為 `ScanCtx`（私有 ring + staging snapshot，定義在 `usage_scan.h`），任務完成後由 loop 執行緒在 `gPollMutex` 下發布到 `gSnap`，杜絕撕裂讀。輪詢中每幀顯示 `SYNC` 徽章（P0 時鐘同步左移避讓），成功發布後 `UPD` 3 秒（僅手動翻頁延遲輪詢顯示，維持原行為）。輪詢中長按 BOOT 排隊到 `gPortalPending`，結束後才開 portal。portal 結束後自動補一次背景輪詢。工作區 `gPollCtx` 用 `ps_malloc` 配置在 PSRAM（8 MB OPI，維持原設定）。
- **事故教訓（首燒即 crash-reboot 迴圈）**：`ps_malloc` 是裸記憶體，`UsageSnapshot::src[i].name`（`const char*`）不會被初始化，發布時垃圾指標被拷進 `gSnap`，`logCombinedForecasts()` 的 `%s` 立即 LoadProhibited 崩潰（EXCVADDR `0x02a9c2ac`，backtrace 落在 `_svfprintf_r`；板子重開後 autoConnect portal 畫面讓螢幕停在 `WIFI SETUP`）。修復：`scanCtxReset()` 對 staging snapshot 做 placement-new 重跑成員初始化。**反例：凡是 `ps_malloc` 配置的 C++ 物件，必須 placement-new 或 memset 後手動賦初值，否則含指標成員的結構必崩。**
- **驗證**：重燒後 Serial 完整序列 `poll ok → P0 六行預測正常列印`，無 Guru Meditation；實機操作確認 SYNC 徽章期間短按 BOOT 立即換頁（Serial 同步出現 `(BOOT)` 換頁）、`SYNC → UPD` 徽章序列正確。耗電結構不變：radio 仍在每次輪詢後關閉，僅新增一個輪詢期間存活的 task（8 KB stack），輪詢頻率與時長未動。

### 2026-11-27：KEY 鍵綁定直跳 P2；PWR 鍵確認不可由韌體讀取

- **KEY（GPIO18）**：短按（≥30 ms，放開時觸發）直接跳到 P2（Claude 10 天圖），同 BOOT 翻頁一樣重設 5 分鐘自動翻頁計時，並在 10 秒無操作後補一次背景輪詢。無長按功能。只新增 `digitalRead` 輪詢（隨 `loop()` 每 50 ms），刷新、Wi-Fi、輪詢頻率與耗電均不變。2026-11-27 使用者實機確認正常。
- **PWR 鍵（schematic Key3）**：接到開關機 IC U3 `ECJ23001-4FCBD6` 的 KEY 腳，OUTH 只驅動電源路徑，**沒有接到任何 ESP32 GPIO**，韌體無法偵測「開機後按一下 PWR」。
- **SYNC 慢的原因（量測）**：`/data` 每次回傳自 2026-07-26 起的完整歷史，7,678 點、5,504,141 bytes；韌體只用最後一點 + 最近 10 天。Mac 端 curl 下載僅約 1.9 秒，ESP32 端要 Wi-Fi 重連 + TLS + 逐 byte 掃描 5.5 MB，約 30–60 秒以上。伺服器支援 gzip（壓縮後 209,635 bytes），但不支援 `?days=` 等過濾參數。待決：在 usage-web 加只回最近 N 天的端點，或韌體端解 gzip。
- **SYNC 修正（同日完成）**：usage-web 新增 `/data?days=N`（keyboardMaestro commit 27fa966，已部署 Contabo；起算點為雲端最新 ts_epoch − N 天，非法值靜默回完整歷史）。韌體 `DATA_URL` 改為 `/data?days=11`（比 10 天趨勢窗多 1 天緩衝，本地 10 天切窗保留為第二道防線），並在下載 >2 MB 或 >2000 點時 Serial 印 `WARN`。`pointCount` 僅用於 log 與 `>0` 判斷，語義改為近 11 天點數不影響功能；0 點 trigger/retry 恢復路徑未動。
- **驗證**：flash.sh exit 0；重置後 Serial：`stream scanned 708750 bytes, pts=897 trend=501`、`poll ok ... C=98 H=66 A=41 G=91 O=80 Y=96`，HTTP 200 到 `poll ok` 約 6.3 秒、Wi-Fi 連上到 radio off 約 7.3 秒（原 30–60 秒以上）。Wi-Fi 開啟時間大幅縮短，每次輪詢耗電下降；輪詢頻率、刷新、翻頁策略不變。KEY→P2 待使用者實機按鍵確認。未 commit／push。
- **SYNC 耗時量測（同日，暫時性 `measure:` Serial log）**：連續兩次重開機輪詢（`/data?days=11`，708,992 bytes）：TLS+GET+headers 1.15／1.21 秒；body 牆鐘 7.9／13.9 秒，其中掃描器 CPU 僅 348 ms、`readBytes` 11 ms，其餘 7.6／13.6 秒都在等網路資料（`available()==0`），實效 91／51 KB/s。結論：瓶頸是 Wi-Fi／TLS 傳輸速率，不是解析；兩次差近一倍顯示受無線環境影響大。下一步候選：下載期間 `WiFi.setSleep(false)`（現為 modem sleep）或 gzip 減少傳輸量。開機輪詢沿用 setup 已連線的 Wi-Fi，冷連線時間（`measure: wifi connect`）尚未取得，需等 15 分鐘排程輪詢或 KEY 延遲輪詢。
- **`setSleep(false)` 實驗（同日，已還原）**：下載期間關 modem sleep，三次 body 實效 88／78／84 KB/s、poll 總計 9.2／10.5／9.5 秒，與開 sleep 時（91／51，還原後再測 50 KB/s；平均約 64 vs 83 KB/s）相比可能略快、波動較小，但樣本少（3 vs 3）且未達質變，先還原。推論瓶頸是 lwIP TCP 接收窗：預編譯 arduino-esp32 3.3.7 的 `CONFIG_LWIP_TCP_WND_DEFAULT=5760`，吞吐 ≈ 5.7 KB／RTT（Mac 到伺服器 TCP 連線約 38 ms，ESP32 經 Wi-Fi 更高），與量到的 ~80–90 KB/s 吻合。要改窗大小需自建 arduino-esp32 lib，成本高；有效手段是減少傳輸 bytes（gzip，709 KB → 26 KB）。
- **gzip 串流解壓（同日完成）**：由 agy（delegate，gemini-4-neon-high，可寫，僅改 `aiusage_home.ino`）實作、Claude 審核。請求改 HTTP/1.1 + `setAcceptEncoding("gzip")`（arduino-esp32 3.3.7 的 `addHeader` 會靜默丟掉 `Accept-Encoding`，`useHTTP10(true)` 也不送），`setReuse(false)`；自帶 chunked 解碼（`HttpBodyReader`）；依 `Content-Encoding` 分派 gzip／identity（伺服器不壓縮時退回原路徑）。gzip 路徑：RFC 1952 header 解析 → ROM miniz `tinfl` raw deflate（32 KB 環狀字典 + 4 KB 輸入，約 47 KB 配在 PSRAM、RAII 釋放）→ 逐 byte `scanFeed`；驗 trailer CRC32（`mz_crc32`）與 ISIZE，失敗沿用上一份畫面。
- **審核抓到的坑（首燒三次全失敗 `gzip trunc`）**：資料已完整解出 710,480 bytes，但 ROM 內建的舊版 miniz `tinfl` 在 DONE 前把 trailer 位元組吞進 bit buffer 且不退還，導致讀不到 8-byte trailer。修法：`GzipInput` 保留 body 最後 8 bytes 的滾動副本，DONE 後排空剩餘輸入再用該副本驗證。**反例：用 ROM tinfl 解 gzip 時不可假設 DONE 後輸入緩衝仍留有完整 trailer。**
- **實測（連續 3 次重開機）**：comp 26,457 → uncomp 710,480 bytes，CRC／ISIZE 通過；body 581／588／744 ms（scan 約 365 ms、inflate 約 29 ms、網路等待 180–360 ms）；TLS+GET 約 0.94–1.03 秒；**poll 總計 1.5–1.7 秒**（identity 時 9–15 秒）。Wi-Fi 開啟時間再大幅縮短，每次輪詢耗電下降；輪詢頻率、刷新、翻頁、trigger/retry 流程不變。identity 退回路徑與 chunked 路徑僅經審閱與編譯，未在實機觸發。`measure:` log 仍保留。未 commit／push。
- **互動節奏微調（同日）**：冷連線實測（手動翻頁觸發）：Wi-Fi 連線 1.2 秒（RSSI −47）＋ TLS 1.15 秒＋ body 0.56 秒，poll 總計 2.9 秒；使用者體感「約 6 秒」主要是 SYNC 後接 3 秒 `UPD` 徽章。`UPD` 改 1 秒（`UPD_BADGE_MS`），手動翻頁後延遲輪詢 `MANUAL_POLL_DELAY_MS` 10→5 秒。連續翻頁仍只在最後一次按鍵後觸發一次；5 秒窗較短，快速瀏覽時多觸發輪詢的機率略增，但每次輪詢已降到約 3 秒，耗電影響小。15 分鐘排程輪詢不變。實機燒錄後開機輪詢正常。

### 2026-11-27：頁序與 ALL DETAIL 欄序調整

- 使用者指定新頁序：P0 Claude、P1 Trend、P2 Alston、P3 Chihyi、P4 Grok、P5 Ollama、P6 AGY、P7 ALL DETAIL；ALL DETAIL 欄序改為 Claude／Alston／Chihyi／Grok／Ollama／AGY。開機第一頁因此是 Claude 圖；KEY 改跳 P0（Claude，跟內容走）。
- 實作：新增 `PAGE_VIEW[]`（頁碼 → 畫面）與 `COMBINED_COL_ORDER[]`（欄位 → 來源索引），`SRC_KEYS` 來源索引不動，所以 P1 趨勢線型、Serial `P0 ...` 預測 log 順序（仍以來源索引）不變；單源圖頁碼標示改用 `gPage`。去掉 `CHART_PAGE_BASE`。
- 同日再調整：P1 與 P7 對調，最終頁序為 P0 Claude、P1 ALL DETAIL、P2 Alston、P3 Chihyi、P4 Grok、P5 Ollama、P6 AGY、P7 Trend（`PAGE_VIEW = {0, COMBINED, 2, 1, 3, 4, 5, TREND}`）。
- R1 400×300、1bpp、刷新與輪詢策略不變；實機燒錄後開機輪詢正常。`ui/aiusage-wireframe.html` 尚未同步新頁序（NEED_REVIEW）。
- wireframe 同步（同日）：上方 NEED_REVIEW 已處理。`ui/aiusage-wireframe.html` 依新頁序重排 8 個 panel（頁碼、標籤、toolbar、說明同步；互動延遲更新改寫為 5 秒），`ui/all-detail-d.svg`／`all-detail-d-partial.svg` 以像素層級交換第 2、3 欄內容（x=73–135 ↔ 137–199，分隔線不動），保留 2026-11-26 快照數值；Chrome headless 截圖確認版面正常。建議歸檔上方 NEED_REVIEW 標記。
