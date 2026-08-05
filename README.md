# ESP32-S3 RLCD-4.2 — 螢幕設計與功能專案

針對 **Waveshare ESP32-S3-RLCD-4.2** 開發板，規劃並實作 **4.2" 全反射式 RLCD** 的 UI 設計、裝置功能與韌體。

硬體像「可快速刷新的電子紙」：無背光、靠環境光可讀、單色 300×400、板載 Wi-Fi/BLE、雙麥克風、喇叭、溫濕度、RTC 與 18650 電池。

**進度與 agent 約定：** [`AGENTS.md`](AGENTS.md)（§6 技術棧 / §7 進度與待辦）

---

## 現況（2026-08-06）

| 項目 | 狀態 |
| --- | --- |
| 框架 | **Arduino + U8g2**（已鎖定） |
| Hello 螢幕 | ✅ `firmware/hello_rlcd` |
| AI 用量儀表 | ✅ `firmware/aiusage_home` — P0/P1/P2/P3 已上板可用 |
| 資料源 | `https://aiusage-web.zeabur.app/data`（週剩餘 % = 100 − used） |
| 換頁 | **短按 BOOT**；長按 3s 配網（AP `AIUsage-RLCD`，僅 2.4 GHz） |
| 電池粗估 | 短測 ~1.14 %/h → 滿電約 3.5–4 天（長測待做，見 AGENTS §6.3） |

### 燒錄

```bash
FQBN='esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=huge_app,FlashSize=16M,PSRAM=opi'
PORT='/dev/cu.usbmodem11401'   # arduino-cli board list

arduino-cli compile --fqbn "$FQBN" --libraries "$HOME/Documents/Arduino/libraries" firmware/aiusage_home
arduino-cli upload -p "$PORT" --fqbn "$FQBN" firmware/aiusage_home
arduino-cli monitor -p "$PORT" -c baudrate=115200
```

WiFi：首次用 AP **`AIUsage-RLCD`** 配網（僅 2.4 GHz；WiFiManager 記在 NVS），或複製 `secrets.h.example` → `secrets.h`（已 gitignore）。
BOOT 靠 USB 側鍵（不是 PWR）；螢幕需環境光。
---

## 硬體速覽

| 項目 | 規格 |
| --- | --- |
| MCU | ESP32-S3-WROOM-1-N16R8（雙核 240 MHz） |
| 記憶體 | 16 MB Flash + 8 MB Octal PSRAM |
| 螢幕 | 4.2" RLCD，300×400，ST7305，SPI，黑白 1bpp |
| 感測 | SHTC3 溫濕度、PCF85063 RTC、電池 ADC |
| 音訊 | ES8311 DAC + ES7210 ADC、雙 mic、喇叭 amp |
| 其他 | TF 卡、BOOT/KEY/PWR、Type-C、2×8 擴充座 |

完整規格與腳位：

- [`docs/specs/HARDWARE-SPEC.md`](docs/specs/HARDWARE-SPEC.md)
- [`docs/specs/PINOUT.md`](docs/specs/PINOUT.md)
- [`docs/specs/SOURCES.md`](docs/specs/SOURCES.md)

官方：

- 產品頁：https://www.waveshare.com/esp32-s3-rlcd-4.2.htm
- Docs：https://docs.waveshare.com/ESP32-S3-RLCD-4.2
- 範例：https://github.com/waveshareteam/ESP32-S3-RLCD-4.2
- 參考實作：[ClaudeSlate](https://github.com/HarryXin0919/ClaudeSlate)（同板、Arduino+U8g2）

---

## 專案目標

1. **UI / 視覺設計** — 單色 RLCD 的資訊層次、字型、版面、頁面流  
2. **核心功能** — 時鐘、用量儀表、感測、電池、IoT  
3. **可選進階** — 語音、Home Assistant / MQTT  
4. **低功耗策略** — always-on 顯示、分級刷新  

### 設計原則

| 原則 | 說明 |
| --- | --- |
| 單色優先 | 黑/白與反白建立層級 |
| 大字可讀 | 桌面掃讀優先 |
| 少全屏刷新 | 儀表 30s–1min；互動才即時 |
| 環境光友善 | 無背光，暗處不可讀是正常 |
| 資訊密度克制 | 400×300，一屏一事 |

---

## 功能藍圖

### Phase 0 — 基建 ✅

- [x] SPEC、`AGENTS.md`、`README.md`
- [x] 框架 Arduino + U8g2
- [x] aiusage wireframe（`ui/`）
- [x] Hello RLCD
- [x] aiusage P0/P1/P2/P3 + BOOT 翻頁

### Phase 1 — 打磨與感測（下一步）

- [ ] 電池長測（完整放電週期）
- [ ] UX：P1 間距、P2 多線可讀性、反顯（`INVERT_DISPLAY`）
- [ ] 離線/stale 與 HTTPS 穩定度（大 JSON ~60KB）
- [ ] SHTC3 室溫濕度
- [ ] KEY 第二操作

### Phase 2+

- [ ] 天氣 / 本機 proxy（ClaudeSlate 式）
- [ ] MQTT / HA
- [ ] 語音（可選）
- [ ] 可選：`aiusage_home` 拆檔（ui / net / data）
---

## 目錄結構

```text
.
├── AGENTS.md                 # agent 約定、技術棧、進度與待辦
├── README.md
├── .gitignore                # 含 firmware/**/secrets.h
├── docs/specs/               # 硬體 SPEC + PDF
├── ui/                       # 單色 wireframe
│   └── aiusage-wireframe.html
└── firmware/
    ├── hello_rlcd/           # 最小顯示驗證
    └── aiusage_home/         # AI 週剩餘儀表（主 sketch）
```
---

## 顯示驅動速查

```cpp
#include <U8g2lib.h>
#include <SPI.h>

#define RLCD_SCK  11
#define RLCD_MOSI 12
#define RLCD_DC   5
#define RLCD_CS   40
#define RLCD_RST  41

U8G2_ST7305_300X400_F_4W_HW_SPI u8g2(U8G2_R1, RLCD_CS, RLCD_DC, RLCD_RST);

void setup() {
  SPI.begin(RLCD_SCK, -1, RLCD_MOSI, RLCD_CS);
  u8g2.begin();
  // ...
}
```

需求：`arduino-esp32 ≥ 3.3.0`，`U8g2 ≥ 2.36.19`。

| 用途 | GPIO |
| --- | ---: |
| Display SPI | 11 / 12 / 5 / 40 / 41 |
| I²C | 13 / 14 |
| BOOT / KEY | 0 / 18 |
| Battery ADC | 4 |
| Speaker amp | 46 |

---

## 開發注意

1. 插拔 USB / 電池時**勿以螢幕受力**  
2. RLCD 暗處看不清是正常現象  
3. 喇叭需 **GPIO46 HIGH**  
4. PSRAM：**octal 80 MHz**  
5. 勿提交 `secrets.h`  

Agent 約定與待辦：[`AGENTS.md`](AGENTS.md) §6–§7。
