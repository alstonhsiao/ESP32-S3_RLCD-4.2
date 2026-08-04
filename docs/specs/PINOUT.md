# ESP32-S3-RLCD-4.2 GPIO Pinout

> 彙整自 Waveshare ESPHome 官方教學與 RLCD 驅動文件。  
> 以電路圖 PDF 為最終依據：`ESP32-S3-RLCD-4.2-schematic.pdf`

---

## 已佔用 GPIO（板載功能）

| GPIO | 功能 | 匯流排 / 備註 |
| ---: | --- | --- |
| 0 | BOOT 按鍵 | 低態有效；strapping / 下載模式 |
| 4 | Battery ADC | 約 3× 分壓，軟體需 ×3 |
| 5 | Display DC | ST7305 Command/Data |
| 8 | I²S DOUT | 喇叭（ES8311） |
| 9 | I²S BCLK | 共用 I²S |
| 10 | I²S DIN | 麥克風（ES7210） |
| 11 | SPI CLK | Display SCK |
| 12 | SPI MOSI | Display MOSI |
| 13 | I²C SDA | SHTC3 / ES8311 / ES7210 / PCF85063 |
| 14 | I²C SCL | 同上 |
| 16 | I²S MCLK | 音訊 master clock |
| 18 | KEY 按鍵 | 低態有效，使用者自訂 |
| 40 | Display CS | ST7305 chip select |
| 41 | Display RESET | ST7305 reset |
| 45 | I²S LRCLK | Word select |
| 46 | Speaker Amp Enable | **高準位才出聲** |

---

## 顯示 SPI 摘要

```
SCK  = GPIO11
MOSI = GPIO12
DC   = GPIO5
CS   = GPIO40
RST  = GPIO41
（無 MISO / 無背光 BL）
```

Arduino 初始化範例：

```cpp
SPI.begin(11, -1, 12, 40);  // SCK, MISO(-1), MOSI, CS
```

---

## I²C 裝置地址

| 裝置 | 地址 | 說明 |
| --- | --- | --- |
| SHTC3 | 0x70 | 溫濕度 |
| ES8311 | 0x18 | 音訊 DAC |
| ES7210 | 0x40 / 0x42 | 音訊 ADC |
| PCF85063 | 見電路圖 / 掃描 | RTC |

Bus: SDA=GPIO13, SCL=GPIO14

---

## I²S 音訊摘要

```
DOUT   = GPIO8   // Speaker
DIN    = GPIO10  // Mic
BCLK   = GPIO9
MCLK   = GPIO16
LRCLK  = GPIO45
AMP_EN = GPIO46  // must be HIGH
```

---

## 擴充座

板載 **2 × 8 PIN、2.54mm pitch** 母座，引出未使用的 GPIO / 電源等。  
實際可用腳位請對照電路圖 PDF，避免與上表衝突。

---

## 電源相關

| 項目 | 說明 |
| --- | --- |
| USB | Type-C 5V（燒錄 + 供電） |
| 主電池 | 18650 座 + 充放電管理 |
| CHG LED | 充電中亮，充滿熄滅 |
| WRN LED | 電池反接常亮 |
| RTC 電池 | PH1.0，**僅可充電型 RTC 電池** |
| PWR 鍵 | 長按關機、短按開機 |
