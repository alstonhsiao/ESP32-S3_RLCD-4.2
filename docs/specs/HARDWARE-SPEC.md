# ESP32-S3-RLCD-4.2 Hardware Specification

> 資料來源：Waveshare 官方產品頁、Documentation Platform、ESPHome 範例、電路圖 PDF。  
> 擷取日期：2026-08-04  
> 詳見 [SOURCES.md](./SOURCES.md)

---

## 1. 產品總覽

| 項目 | 內容 |
| --- | --- |
| 產品名稱 | ESP32-S3-RLCD-4.2 |
| 製造商 | Waveshare |
| 定位 | 全反射式 RLCD AIoT 開發板 |
| SKU (含 18650) | 33298 — ESP32-S3-RLCD-4.2 |
| SKU (不含電池) | 33507 — ESP32-S3-RLCD-4.2-EN |
| 通訊 | 2.4 GHz Wi-Fi + Bluetooth 5 (LE) |
| 主要應用 | 桌面智慧擺件、電子日曆、AI 語音裝置、IoT 控制、產品原型 |

**產品一句話描述**  
以 ESP32-S3 為核心的全反射螢幕 AIoT 開發板：4.2 吋 RLCD 近似電子紙的閱讀體驗但刷新更快，板載雙麥克風、喇叭、溫濕度感測、RTC、TF 卡與 18650 電池管理。

### 注意事項（官方 Caution）

1. 連接 Type-C 或插拔 18650 時，**不可把螢幕當受力點**。
2. 螢幕為精密易碎件，組裝與搬運需小心，避免撞擊。
3. 因上述不當操作導致的螢幕破裂或顯示異常，不在保固範圍。

---

## 2. MCU / 記憶體

| 項目 | 規格 |
| --- | --- |
| 模組 | ESP32-S3-WROOM-1-N16R8 |
| 架構 | Xtensa 32-bit LX7 雙核心 |
| 主頻 | 最高 240 MHz |
| SRAM | 512 KB |
| ROM | 384 KB |
| Flash | 16 MB（模組內建） |
| PSRAM | 8 MB Octal PSRAM |
| 無線 | 2.4 GHz Wi-Fi 802.11b/g/n + Bluetooth 5 LE（板載天線） |
| USB | Type-C（燒錄 + 日誌） |

> ESPHome / 韌體注意：PSRAM 必須設為 `octal`、`80MHz`，顯示 framebuffer 與音訊緩衝都依賴 PSRAM。

---

## 3. 顯示（RLCD）

| 項目 | 規格 |
| --- | --- |
| 類型 | 全反射式 LCD（Reflective LCD / RLCD） |
| 尺寸 | 4.2 inch |
| 解析度 | **300 × 400**（直立）/ **400 × 300**（橫向旋轉後） |
| 色彩 | 單色黑白（1 bpp） |
| 驅動 IC | **Sitronix ST7305** |
| 介面 | 4-wire SPI（寫入為主，無 MISO） |
| 背光 | **無背光**（靠環境光反射成像） |
| Frame buffer | 300×400÷8 = **15,000 bytes**（約 15 KB） |
| 對比產品定位 | 功耗接近 e-Paper，刷新速度接近一般 LCD |

### RLCD 特性摘要

**優點**

- 無背光 → 靜態功耗極低，適合 always-on 電池裝置
- 毫秒級液晶響應，可動畫 / 即時資料（不像 e-Paper 鬼影）
- 強光下可讀性反而更好
- 紙感閱讀，長時間使用較不刺眼

**限制**

- 暗處幾乎不可見，需外部照明
- 單色、對比度中等
- 控制器與尺寸選擇較一般 LCD 少

### 顯示 SPI 腳位

| 訊號 | GPIO | 說明 |
| --- | --- | --- |
| RLCD_SCK | GPIO11 | SPI Clock |
| RLCD_MOSI | GPIO12 | SPI Data |
| RLCD_DC | GPIO5 | Command / Data |
| RLCD_CS | GPIO40 | Chip Select |
| RLCD_RST | GPIO41 | Reset |

建議 SPI 時脈：Arduino u8g2 範例可到 **24 MHz**；ESPHome 官方範例常用 **1 MHz**（穩定性優先）。

### 推薦圖形庫

| 環境 | 建議 |
| --- | --- |
| Arduino | **U8g2**（`U8G2_ST7305_300X400_*`，需 U8g2 ≥ v2.36.19） |
| ESP-IDF | Waveshare 官方範例 |
| ESPHome | 社群元件 `st7305_rlcd`（kylehase/ESPHome-ST7305-RLCD） |
| 複雜 UI | LVGL monochrome mode |

板卡設定：Arduino 需 **arduino-esp32 ≥ v3.3.0**。

---

## 4. 板載周邊

| 周邊 | 型號 / 介面 | 用途 |
| --- | --- | --- |
| 顯示 | ST7305 / SPI | 4.2" 單色 RLCD |
| 溫濕度 | SHTC3 / I²C `0x70` | 環境監測 |
| RTC | PCF85063(A) / I²C | 即時時鐘、斷電走時（需 RTC 備援電池） |
| 音訊 DAC | ES8311 / I²C + I²S | 喇叭輸出 |
| 音訊 ADC | ES7210 / I²C + I²S | 雙麥克風 + 回聲消除 |
| 喇叭 | MX1.25 2PIN、8Ω 2W | 外接喇叭座 |
| 麥克風 | 雙麥克風陣列 | 降噪 / AEC / 近遠場喚醒 |
| 儲存 | TF Card（FAT32） | 圖片 / 檔案擴充 |
| 電池 | 18650 座 + 充放電管理 | 主電源 |
| RTC 備援 | PH1.0（僅支援可充電 RTC 電池） | RTC 獨立供電 |
| 按鍵 | BOOT、KEY、PWR | 下載模式 / 自訂 / 開關機 |
| 指示燈 | CHG、WRN | 充電完成熄滅；反接常亮 |
| 擴充 | 2×8 2.54mm 母座 | USB/UART/I2C/GPIO 引出 |

### I²C 匯流排

| 訊號 | GPIO |
| --- | --- |
| SDA | GPIO13 |
| SCL | GPIO14 |

預期掃描地址（開機 log）：

- `0x70` — SHTC3
- `0x18` — ES8311
- `0x40` 或 `0x42` — ES7210
- PCF85063 亦在同一 I²C 總線

### I²S 音訊腳位

| 訊號 | GPIO | 說明 |
| --- | --- | --- |
| I2S DOUT | GPIO8 | 喇叭資料（DAC → amp） |
| I2S BCLK | GPIO9 | Bit clock |
| I2S DIN | GPIO10 | 麥克風資料 |
| I2S MCLK | GPIO16 | Master clock |
| I2S LRCLK | GPIO45 | Word select |
| Amp Enable | GPIO46 | **必須拉高** 喇叭才有聲 |

音訊常用取樣：16-bit、16 kHz。

### 按鍵與電源感測

| 訊號 | GPIO | 說明 |
| --- | --- | --- |
| BOOT | GPIO0 | 低態有效；開機按住進入下載模式 |
| KEY | GPIO18 | 低態有效；使用者自訂 |
| Battery ADC | GPIO4 | 分壓約 ×3；軟體需 `×3` 校正 |

電池電量粗算：2.5 V → 0%、4.2 V → 100%（18650 近似線性）。

---

## 5. 板載資源標號（官方 Onboard Resources）

1. **ESP32-S3-WROOM-1-N16R8** — Wi-Fi/BT SoC，16MB Flash + 8MB PSRAM  
2. **ES7210** — ADC，回聲消除  
3. **ES8311** — 低功耗音訊編解碼  
4. **BOOT** — 下載模式  
5. **PWR** — 長按關機、短按開機  
6. **KEY** — 自訂按鍵  
7. **SHTC3** — 溫濕度  
8. **PCF85063** — RTC  
9. **MX1.25 2PIN Speaker** — 喇叭座  
10. **RTC 獨立供電座** — 僅 PH1.0 可充電 RTC 電池  
11. **2×8 2.54mm 母座**  
12. **18650 電池座**  
13. **雙麥克風陣列**  
14. **CHG** — 充滿熄滅  
15. **WRN** — 反接常亮  
16. **Type-C** — 燒錄與 log  
17. **TF 卡槽** — FAT32  

---

## 6. 開發方式

| 框架 | 說明 |
| --- | --- |
| **Arduino IDE** | 入門友善；U8g2 驅動 RLCD；需 arduino-esp32 ≥ 3.3.0 |
| **ESP-IDF** | 官方專業框架；建議 VS Code + Espressif 外掛 |
| **ESPHome** | 適合 Home Assistant 整合；需 external_components 拉 ST7305 |
| **Zephyr** | 有 board support（`esp32s3_rlcd_4_2`） |

官方範例倉庫：  
https://github.com/waveshareteam/ESP32-S3-RLCD-4.2

---

## 7. 包裝內容（Quick Overview）

1. ESP32-S3-RLCD-4.2 主機 ×1  
2. 壓克力支架（2 片）×1  
3. 可選 18650 鋰電池 ×1  
4. 8Ω 2W MX1.25 2PIN 喇叭 ×1  
5. 螺絲起子 ×1  

---

## 8. 本專案設計時的硬體約束（重點）

| 面向 | 約束 |
| --- | --- |
| UI 色彩 | **僅黑白 1bpp**，用反白、區塊、線條建立層次 |
| 解析度 | 400×300 橫向或 300×400 直立，資訊密度有限 |
| 可視環境 | 需環境光；夜間場景要外加燈或接受不可讀 |
| 刷新策略 | 可快速刷新，但頻繁全屏更新耗電；儀表板宜 30s–1min |
| 記憶體 | 全屏 buffer 僅 15KB；複雜 UI 可用 partial update / tile |
| 語音 | 喇叭 amp 必須 enable（GPIO46）；雙 mic 適合喚醒與對話 |
| 電池 | Always-on 儀表是強項；語音與 Wi-Fi 才是主要耗電源 |
| 擴充 | 2×8 母座可加外設，但注意與已佔用 GPIO 衝突 |

---

## 9. 本地文件

| 檔案 | 說明 |
| --- | --- |
| [ESP32-S3-RLCD-4.2-schematic.pdf](./ESP32-S3-RLCD-4.2-schematic.pdf) | 官方電路圖 |
| [ST7305-datasheet.pdf](./ST7305-datasheet.pdf) | ST7305 控制器資料手冊 |
| [PINOUT.md](./PINOUT.md) | GPIO 總表 |
| [SOURCES.md](./SOURCES.md) | 來源連結與擷取紀錄 |
