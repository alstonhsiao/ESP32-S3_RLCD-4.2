# AGENTS.md — ESP32-S3 RLCD-4.2

本檔給在此 repo 工作的 AI agent / 開發者使用。  
專案目標：**Waveshare ESP32-S3-RLCD-4.2 的螢幕設計與功能開發**。

---

## 1. 專案定位

| 項目 | 內容 |
| --- | --- |
| 硬體 | Waveshare **ESP32-S3-RLCD-4.2**（SKU 33298 / 33507-EN） |
| 核心 | 4.2" 全反射 RLCD（ST7305，300×400，單色 1bpp） |
| 工作重點 | UI 設計、資訊架構、韌體功能、低功耗策略 |
| 非重點 | 不要為了炫技上彩色 UI 或重繪成一般 LCD 假設 |

硬體規格唯一權威來源（本 repo 內）：

1. `docs/specs/HARDWARE-SPEC.md`
2. `docs/specs/PINOUT.md`
3. `docs/specs/ESP32-S3-RLCD-4.2-schematic.pdf`（最終硬體依據）
4. `docs/specs/ST7305-datasheet.pdf`

官方線上文件：

- https://docs.waveshare.com/ESP32-S3-RLCD-4.2  
- https://www.waveshare.com/esp32-s3-rlcd-4.2.htm  
- 範例：https://github.com/waveshareteam/ESP32-S3-RLCD-4.2  

若線上文件與本機 schematic 衝突，**以 schematic 為準**，並更新 `docs/specs/`。

---

## 2. 硬體不可違反的約束

Agent 寫程式或做設計時必須遵守：

### 顯示

- 解析度：**300×400** 或旋轉後 **400×300**
- 色彩：**僅黑白 1 bpp**（`COLOR_ON` / `COLOR_OFF`）
- 驅動：**ST7305**，4-wire SPI
- 腳位：SCK=11, MOSI=12, DC=5, CS=40, RST=41
- **無背光**；暗處不可讀是正常現象
- 全屏 buffer ≈ **15 KB**，不要假設大 framebuffer / RGB565

### 匯流排

- I²C：SDA=13, SCL=14（SHTC3 `0x70`、ES8311、ES7210、PCF85063）
- I²S：DOUT=8, BCLK=9, DIN=10, MCLK=16, LRCLK=45
- 喇叭 amp：**GPIO46 必須 HIGH 才有聲**
- 電池 ADC：GPIO4，讀值約需 **×3**
- 按鍵：BOOT=GPIO0、KEY=GPIO18（低態有效）

### 記憶體 / 框架

- 8 MB **Octal** PSRAM @ 80 MHz（顯示與音訊依賴）
- Arduino：`arduino-esp32 ≥ 3.3.0`；U8g2 ≥ 2.36.19 才有 ST7305 300×400
- 勿隨意重映射已佔用 GPIO；擴充前查 `PINOUT.md`

### 安全操作

- 文件與註解中提醒：插拔 Type-C / 18650 時勿以螢幕受力
- 不建議在文件中鼓勵暴力拆裝或超壓供電

---

## 3. 設計與功能準則

本專案是 **設計 + 功能**，不是 dump demo code。

### UI 設計

- 以桌面/牆面 always-on 儀表為主場景
- 層次靠：字級、反白條、線框、留白；不用假灰階
- 優先可掃讀：時間、溫度、狀態 > 裝飾
- 一屏一事；詳情用翻頁或 KEY 切換
- 新增 UI 時優先放 wireframe / mock 到 `ui/`（若目錄存在）

### 功能優先序

1. 可靠顯示 + 正確方向與對比  
2. 時鐘 / 日曆 + 溫濕度 + 電池  
3. 頁面系統與按鍵交互  
4. 連網（Wi-Fi 狀態、MQTT / HA）  
5. 語音與 AI（可選，耗電大）  

### 功耗

- 顯示靜態很省；避免無意義全屏刷新
- 儀表更新間隔預設 30s–60s 量級，互動時可即時
- 語音、連續 Wi-Fi、高 FPS 動畫需標註為高耗電模式

---

## 4. 建議目錄與檔案約定

```text
docs/specs/     # 硬體 SPEC、PDF、pinout（已建立）
ui/             # 設計稿、wireframe、1bpp icon 說明
firmware/       # 韌體原始碼（依選定框架再建）
assets/         # 字型、點陣圖
```

- 新的硬體事實 → 寫進 `docs/specs/`，不要只留在 chat  
- 重要決策與進度 → 更新本檔 `AGENTS.md` 或 `README.md` 的對應區塊  
- **禁止**使用全域 `memory/` 系統；專案記憶一律寫在本 repo（預設 `AGENTS.md`）

---

## 5. 實作時的檢查清單

改顯示相關程式前：

- [ ] 腳位是否與 `PINOUT.md` 一致  
- [ ] buffer 是否按 1bpp 計算  
- [ ] 旋轉方向是否明確（R0/R1/R2/R3 或 width/height）  
- [ ] 是否在暗光假設下誤用「必須可見」的 UX  

加感測器 / 音訊前：

- [ ] I²C 地址是否衝突  
- [ ] PSRAM 模式是否 octal  
- [ ] 喇叭是否 enable GPIO46  
- [ ] 電池 ADC 是否 ×3 與 clamp 0–100  

提交或說明變更時：

- [ ] 說明「設計意圖」而不只列檔名  
- [ ] 標註是否影響功耗或刷新策略  

---

## 6. 技術棧（已鎖定）

| 項目 | 選定 |
| --- | --- |
| **框架** | **Arduino + U8g2**（已鎖定 2026-08-04） |
| 顯示驅動 | Waveshare `ST7305_U8g2`（自官方 demo 複製，參考 ClaudeSlate） |
| 板級 FQBN | `esp32:esp32:esp32s3`，USB CDC On Boot、Huge APP、Flash 16MB |
| JSON / HTTP | `HTTPClient` + `ArduinoJson` |
| Wi-Fi 配網 | **WiFiManager**（NVS 記住上次配網；可被 `secrets.h` 覆寫） |
| 函式庫 | U8g2（≥ 2.36.19）、ArduinoJson、WiFiManager |
| 備選（暫不採用） | ESP-IDF（後期產品化）、ESPHome（HA 向，不適複雜儀表 UI） |

**本機工具（2026-08-04）**：`arduino-cli 1.5.0` + `esp32:esp32 3.3.7`；未偵測到 ESP-IDF / ESPHome / PlatformIO。

| 項目 | 值 |
| --- | --- |
| USB 裝置 | `ESP32 Family Device`（PORT 以 `arduino-cli board list` 為準） |
| 常見 PORT | `/dev/cu.usbmodem11401` |
| 本機板 MAC | `a4:cb:8f:d0:6c:00`（識別用，非硬體通用規格） |

韌體目錄：`firmware/` — Arduino sketch。  
- ✅ `firmware/hello_rlcd/` — Hello 燒錄成功  
- ✅ `firmware/aiusage_home/` — P0/P1/P2/P3 + BOOT 翻頁（P3 Daily Pace 2026-08-05）  
UI 設計稿：`ui/aiusage-wireframe.html`  

### 燒錄 / 序列埠速查

```bash
FQBN='esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=huge_app,FlashSize=16M,PSRAM=opi'
PORT='/dev/cu.usbmodem11401'   # board list 確認
arduino-cli compile --fqbn "$FQBN" --libraries "$HOME/Documents/Arduino/libraries" firmware/aiusage_home
arduino-cli upload -p "$PORT" --fqbn "$FQBN" firmware/aiusage_home
arduino-cli monitor -p "$PORT" -c baudrate=115200
```

### 操作備忘

| 操作 | 說明 |
| --- | --- |
| 換頁 | 短按 **BOOT**（靠 USB 側鍵，不是 PWR） |
| 重配網 | 長按 BOOT ~3 秒 → 手機連 `AIUsage-RLCD` → `192.168.4.1` |
| WiFi | 僅 **2.4 GHz**；WiFiManager 記在 NVS |
| 寫死 WiFi | 複製 `secrets.h.example` → `secrets.h`（已 gitignore，勿提交） |
| 看螢幕 | 需環境光（無背光） |

### 開機復原檢查

1. 讀本檔 §6–§7  
2. USB 插板 → `arduino-cli board list`  
3. 螢幕是否仍為 aiusage（否則重燒 `firmware/aiusage_home`）  
4. 短按 BOOT 確認 P0→P1→P2→P3  
5. 再決定 UX / 感測 / 穩定度工作
---

## 6.1 重要參考專案：ClaudeSlate（必讀）

> **之後做 UI / 用量儀表 / 代理架構時，優先參考此 repo。**

| 項目 | 內容 |
| --- | --- |
| Repo | https://github.com/HarryXin0919/ClaudeSlate |
| 硬體 | 同一塊 **Waveshare ESP32-S3-RLCD-4.2** |
| 韌體框架 | **Arduino**（`.ino`）+ **U8g2** + Waveshare 的 `ST7305_U8g2` 驅動 |
| 板級設定 | ESP32S3 Dev Module、USB CDC On Boot、Huge APP、Flash 16MB；`esp32 core 3.x` |
| 資料架構 | **PC 端 Python proxy** → 精簡 JSON → 螢幕 Wi-Fi 輪詢（非瀏覽器渲染網頁） |
| 顯示內容 | 8 頁：時鐘/天氣、Claude/Codex 用量、7 天柱圖、週趨勢、室內溫濕度、電池 |
| 可借鏡 | 400×300 單色版面、進度條/柱圖/折線、BOOT 翻頁、captive portal 配網、UDP 發現 proxy、軟體反顯黑字白底（`INVERT_DISPLAY`） |

可複用概念（不要整包抄，按本專案需求裁切）：

1. **Proxy 模式**：本機敏感資料（OAuth、log）只在 PC 處理，裝置只收衍生 JSON  
2. **多頁儀表 + BOOT 切換**  
3. **U8g2 單色資訊設計**（大數字、bar、sparkline、狀態列）  
4. **輪詢節奏**：畫面 ~10s 重繪、資料 ~60s 拉一次  
5. **離線/stale 狀態顯示**  

相關（同作者）：ClaudeOrb — 同一概念的「球體」型態，可當 UX 靈感。

---

## 6.2 資料源：aiusage-web（已實作 P0–P3）

| 項目 | 內容 |
| --- | --- |
| Web UI | https://aiusage-web.zeabur.app/ |
| API | `GET /data`、`GET /health` |
| 語意 | **週剩餘 % = 100 − used_weekly_pct** |
| 來源 | `claude` / `codex` / `grok` / `ollama` |
| 韌體 | `firmware/aiusage_home/` |
| 上游 ingest（非本 repo） | 本機 `keyboardMaestro/autousage` |

**頁面**：P0 Home（時鐘 + 四源）→ P1 Detail（WEEK / 5H / RESET，低剩餘反白）→ P2 Trend（最多 40 點折線）→ P3 Daily Pace（24h 日額 / 週結束 / 5h 結束）。

**注意**：不可在 RLCD 上原樣渲染彩色網頁；已改為 U8g2 單色儀表。全量 JSON ~60KB，後續可考慮精簡 API（最新 + N 點）。
---

## 6.3 電池續航實測（18650）

> 電量來自 GPIO4 ADC 百分比（非庫侖計），僅供粗估。後續需做**長測**再校正。

### 短區間觀測（2026-08-05 夜 → 2026-08-06 晨）

| 項目 | 數值 |
| --- | --- |
| 韌體 / 模式 | `firmware/aiusage_home/`，always-on 儀表 + Wi-Fi 輪詢 |
| 起點 | **22:50**，**87%** |
| 終點 | **07:35**，**77%** |
| 經過 | **8 h 45 min**（8.75 h） |
| 耗電 | **10%** |
| 平均耗電率 | **≈ 1.14 %/h**（≈ **27–28 %/天**） |
| 滿電理論續航 | **≈ 87.5 h ≈ 3.6 天**（約 3.5–4 天） |
| 當時 77% 剩餘 | **≈ 67 h ≈ 2.8 天** |

### 長測計畫（待做）

- [ ] 從滿電（或已知高電量）跑到低電／關機，記錄時間戳 + 電量 %
- [ ] 標註期間：Wi-Fi 是否常連、輪詢間隔、是否有互動／語音
- [ ] 建議至少 **1 個完整放電週期**（最好 2 次對照）
- [ ] 結果補回本節表格；若與 3.6 天差很多，再查輪詢節奏／刷新策略

**誤差提醒**：ADC 中段曲線較平、日間使用可能更耗電、低電末段掉得較快。

---

## 7. 進度紀錄

| 日期 | 內容 |
| --- | --- |
| 2026-08-04 | SPEC 基建；ClaudeSlate 參考；鎖定 Arduino+U8g2 |
| 2026-08-04 | wireframe；Hello RLCD 燒錄 OK |
| 2026-08-04 | aiusage_home P0/P1/P2 + BOOT 翻頁；使用者確認完成 |
| 2026-08-04 | 整理 README/AGENTS；session handoff 初版 |
| 2026-08-05 | P3 Daily Pace（表格式 24h 日額 / 週結束 / 5h 結束）；wireframe P3/P3b |
| 2026-08-06 | 電池短測：87%→77% / 8.75h → 估滿電約 3.6 天；長測待做（§6.3） |
| 2026-08-06 | 將 `handoff20260804.md` 要點併入本檔與 README 後刪除 |

### 待辦（給後續 agent）

- [x] 框架 / Hello / aiusage 多頁（P0–P2）  
- [x] P3 建議一天使用額度（方案 B 表格）  
- [ ] **電池長測**（滿電→低電完整週期；見 §6.3）  
- [ ] **UX 打磨**
  - P1 四列在 300 高是否擁擠 → 微調間距/字級  
  - P2 四線重疊可讀性 → 可改「單源切換」或只畫 2 源  
  - 軟體反顯（黑字白底）：參考 ClaudeSlate `INVERT_DISPLAY`  
- [ ] **穩定度**
  - HTTPS / 大 JSON（~60KB）記憶體與 timeout  
  - 離線/stale 狀態文案與重試  
  - 可選：後端精簡 API（最新 + N 點）  
- [ ] **功能擴充**
  - 板載 SHTC3 溫濕度（ClaudeSlate 底欄 Room）  
  - KEY（GPIO18）第二操作（只刷資料、鎖定某頁等）  
  - 天氣（Open-Meteo）或 ClaudeSlate 式本機 proxy  
- [ ] **工程**（可選）
  - 確認 GitHub remote、`secrets.h` 未進版控  
  - `firmware/aiusage_home` 拆檔（ui / net / data）  

### 刻意未做 / 不做假設

- 未在螢幕上「開瀏覽器」渲染網頁（技術上不可行）  
- 未上 ESP-IDF / ESPHome 主線  
- 未做語音 / 喇叭  
- 未提交 `firmware/**/secrets.h`  

**開場**：讀本檔 §6–§7，再依待辦選一項開工。
---

## 8. 回應與協作風格

- 使用繁體中文與使用者溝通（程式識別子可用英文）  
- 改動保持小而可驗證；先顯示與腳位，再堆功能  
- 不捏造未在 SPEC / 電路圖出現的腳位或晶片  
- 不寫 exploit / 攻擊工具；本專案僅裝置設計與韌體  
- 破壞性操作（強制 push、刪除大量檔案）前先確認  

---

## 9. 快速參考連結

| 資源 | 路徑 / URL |
| --- | --- |
| 硬體總規格 | `docs/specs/HARDWARE-SPEC.md` |
| GPIO | `docs/specs/PINOUT.md` |
| 來源 | `docs/specs/SOURCES.md` |
| 電路圖 | `docs/specs/ESP32-S3-RLCD-4.2-schematic.pdf` |
| ST7305 | `docs/specs/ST7305-datasheet.pdf` |
| 專案說明 | `README.md` |
