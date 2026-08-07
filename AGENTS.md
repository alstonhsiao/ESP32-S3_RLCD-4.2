# AGENTS.md — ESP32-S3 RLCD-4.2

本檔是本 repo 的 agent Hub：先讀本檔，再依「快速地圖」進入對應 Spoke 或模組。專案目標是 Waveshare ESP32-S3-RLCD-4.2 的單色 RLCD UI、裝置功能與低功耗韌體；不要把它當成一般彩色 LCD demo 專案。

## 快速地圖

| 要做什麼 | 先讀哪個檔案 |
| --- | --- |
| 確認硬體事實、顯示方向或 GPIO | `docs/specs/INDEX.md` → `HARDWARE-SPEC.md`／`PINOUT.md`／schematic |
| 了解 Arduino 版本、編譯、燒錄、配網與復原 | `docs/firmware-operations.md`、`firmware/INDEX.md` |
| 修改或除錯 aiusage 主韌體 | `firmware/aiusage_home/INDEX.md` → `aiusage_home.ino` |
| 調整 Hello 顯示驗證 | `firmware/hello_rlcd/README.md`、`firmware/hello_rlcd/hello_rlcd.ino` |
| 調整版面、頁面或單色 wireframe | `docs/ui-and-data.md`、`ui/INDEX.md` |
| 理解 aiusage API、頁面語義與刷新節奏 | `docs/ui-and-data.md` |
| 查電池實測、待辦、歷史決策與刻意不做事項 | `docs/power-and-progress.md` |
| 人類導向的專案總覽 | `README.md` |

各高 token 目錄的 agent 路由見其根目錄 `INDEX.md`；`README.md` 仍是人類使用說明。

## 專案方向與高風險規則

### 硬體權威來源

本 repo 的硬體事實依序以以下本機文件為準：

1. `docs/specs/HARDWARE-SPEC.md`
2. `docs/specs/PINOUT.md`
3. `docs/specs/ESP32-S3-RLCD-4.2-schematic.pdf`（最終硬體依據）
4. `docs/specs/ST7305-datasheet.pdf`

若線上文件與 schematic 衝突，以 schematic 為準，並把已確認的差異更新到 `docs/specs/`；不確定處標 `NEED_REVIEW`。

### 顯示不可違反事項

- 面板是 ST7305、4-wire SPI、單色黑白 1bpp；解析度是 300×400，旋轉後可用 400×300。
- 顯示腳位固定為 SCK=11、MOSI=12、DC=5、CS=40、RST=41；不要在沒有核對 `PINOUT.md` 與 schematic 前重映射。
- 無背光，暗處不可讀是正常硬體限制；不要設計「必須在暗處可見」的 UX。
- 全屏 1bpp buffer 約 15 KB；不要假設 RGB565 或大型彩色 framebuffer。
- 修改顯示程式時必須明確記錄旋轉方向（R0/R1/R2/R3 或 width/height）與刷新策略。

### 匯流排、電源與記憶體不可違反事項

| 資源 | 固定事實 |
| --- | --- |
| I²C | SDA=13、SCL=14；SHTC3 `0x70`、ES8311、ES7210、PCF85063 共用，新增裝置前先查地址衝突 |
| I²S | DOUT=8、BCLK=9、DIN=10、MCLK=16、LRCLK=45 |
| 喇叭 | amp enable 是 GPIO46，必須拉 HIGH 才有聲 |
| 電池 | ADC 是 GPIO4，讀值約需乘 3，百分比要 clamp 到 0–100 |
| 按鍵 | BOOT=GPIO0、KEY=GPIO18，皆低態有效；BOOT 另涉及下載模式 |
| PSRAM | 8 MB Octal、80 MHz；顯示與音訊緩衝依賴它 |
| 框架 | Arduino + U8g2；`arduino-esp32` 至少 3.3.0，U8g2 至少 2.36.19 |

Type-C 或 18650 插拔時勿以螢幕受力；文件不得鼓勵暴力拆裝或超壓供電。

### 已鎖定的產品與功能方向

- 板級設定是 `esp32:esp32:esp32s3`、USB CDC On Boot、Huge APP、Flash 16 MB、OPI PSRAM。
- UI 以桌面／牆面 always-on 儀表為主；掃讀順序是時間、溫度、狀態，再到裝飾。
- 層次靠字級、反白條、線框與留白，不使用假灰階；一屏一事，詳情用翻頁或 KEY。
- 功能優先序：可靠顯示與方向對比 → 時鐘／日曆／溫濕度／電池 → 頁面與按鍵 → Wi-Fi／MQTT／HA → 語音與 AI。
- 靜態畫面要避免無意義全屏刷新；儀表預設約 30–60 秒級更新，互動時才即時。連續 Wi-Fi、語音、高 FPS 動畫都要標註高耗電。
- aiusage 的核心語義是「週剩餘 % = `100 − used_weekly_pct`」；不要在 RLCD 上原樣渲染彩色網頁。
- `secrets.h` 可以在本機覆寫 Wi-Fi，但不得提交；只提交 `secrets.h.example`。

## 實作前檢查清單

### 顯示變更

- [ ] 腳位與 `docs/specs/PINOUT.md` 一致。
- [ ] buffer 依 1bpp 計算，沒有引入未核准的 RGB framebuffer。
- [ ] 旋轉方向、可視尺寸、對比與環境光假設已寫清楚。
- [ ] 刷新頻率與功耗影響已在回報中標註。

### 感測器或音訊變更

- [ ] I²C 地址沒有衝突，且先核對 schematic。
- [ ] PSRAM 維持 Octal 80 MHz。
- [ ] 喇叭路徑會先 enable GPIO46。
- [ ] 電池 ADC 乘 3 並 clamp 0–100。

### 變更回報

- [ ] 說明設計意圖，不只列檔名或函式名。
- [ ] 說明是否改變資料拉取、畫面刷新、Wi-Fi 連線或電池消耗。
- [ ] 只做小而可驗證的修改；不自動 commit 或 push。

## 文件維護規則

### 文件修改權限

修改任何治理文件前，先聲明該檔屬於哪一級。本次 Hub 重構保留高風險規則原意；後續若要改變其內容，仍須依下表處理。

| 級別 | 範圍 | 規則 |
| ---- | ---- | ---- |
| 🟢 可自行修改 | 各 `INDEX.md` 的檔案清單、快速地圖的路徑、README 使用說明 | 事實性內容，改完在回報中列出即可 |
| 🟡 改前必須先問使用者 | Hub 的高風險規則、不可違反的硬體規則、`docs/specs/HARDWARE-SPEC.md`、`docs/specs/PINOUT.md` 的核心語義 | 即使只是精簡措辭也要先問，不得擅自改寫或弱化 |
| 🔵 只准追加，不准自行刪改 | troubleshooting／事故紀錄檔（本專案暫無）；各文件的 `NEED_REVIEW` 標記 | 新教訓往後追加；認為某條過時，追加「建議歸檔」並提報，不得直接刪除；經使用者明確核准後才搬移歸檔 |

### troubleshooting 升格規則

- 本專案目前沒有 troubleshooting／事故紀錄檔；若建立，完整事故經過一律追加到該檔，並在本段填入實際路徑。
- 符合下列任一條件時「升格」：同類坑第二次發生，或屬高風險事故。
- 升格 = 在 Hub 對應規則後追加一行反例，以及 troubleshooting 條目編號。
- 未升格的教訓留在 troubleshooting 檔即可，不要把 Hub 當事故簿。

### 路徑檢查與瘦身協議

- 路徑檢查：例行維護時，逐一驗證 Hub 與各 `INDEX.md` 中提到的檔案路徑是否存在；失效路徑立即修正，無法確定則標 `NEED_REVIEW`。
- 瘦身觸發：troubleshooting 檔超過約 600 行，或「建議歸檔」標註累積 5 條以上時，列提名表 `| 條目 | 建議 | 理由 |` 交使用者裁決。
- 瘦身執行：獲准條目由 agent 搬移至 `docs/archive/`，搬移不刪除。
- 瘦身判準：區分「場景過時」（可歸檔）與「教訓仍通用」（保留，甚至升格），提名表逐條說明分類。

## 派工與停損

1. 派工門檻：預估要讀超過 5 個檔案或 50KB、或需要掃整個目錄時，派 subagent，主對話只收結論；低於門檻自己做，不要為小事派工。
2. 派工三件套：每次派 subagent 必須寫明 (1) 目標與動機 (2) 驗收條件 (3) 回報格式——只回結論 + 檔案:行號，長產物落檔傳路徑。
3. 停損線：同一子任務用同一種方法連錯兩次，停止重試；帶完整失敗軌跡（做了什麼、錯誤訊息、已排除什麼）回報使用者，不得換個小花樣試第三次。

本專案的正例：要比較 schematic、PINOUT、HARDWARE-SPEC、主 sketch 與 wireframe 才能決定顯示方向與腳位時，檔案和目錄範圍已超過門檻，應派工並只收結論。

本專案的反例：只需確認某個 `INDEX.md` 的路徑或更新一個快速地圖連結時，先自行做，不應為小型事實修改派工。

## 協作邊界

- 使用繁體中文溝通；程式識別子可用英文。
- 新的硬體事實寫進 `docs/specs/`，重要決策與進度寫進對應模組或 `README.md`，不要只留在對話。
- 禁止使用全域 `memory/`；專案記憶只寫在本 repo。
- 不捏造 SPEC 或 schematic 未出現的腳位、晶片或能力。
- 不寫 exploit／攻擊工具；本專案只做裝置設計與韌體。
- 破壞性操作、強制 push、刪除大量檔案前先取得確認；治理文件只依上方權限規則修改。

## 常用入口

- 硬體總規格：`docs/specs/HARDWARE-SPEC.md`
- GPIO 總表：`docs/specs/PINOUT.md`
- 來源紀錄：`docs/specs/SOURCES.md`
- 電路圖：`docs/specs/ESP32-S3-RLCD-4.2-schematic.pdf`
- ST7305 資料手冊：`docs/specs/ST7305-datasheet.pdf`
- 人類導向專案說明：`README.md`
