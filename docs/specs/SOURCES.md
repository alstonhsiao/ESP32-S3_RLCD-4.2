# Spec 來源與擷取紀錄

| 日期 | 來源 | URL | 用途 |
| --- | --- | --- | --- |
| 2026-08-04 | Waveshare 產品頁 | https://www.waveshare.com/esp32-s3-rlcd-4.2.htm | 產品規格、功能列表、板載資源、版本 SKU |
| 2026-08-04 | Waveshare Docs | https://docs.waveshare.com/ESP32-S3-RLCD-4.2 | 官方產品文件總覽 |
| 2026-08-04 | Waveshare RLCD 教學 | https://docs.waveshare.com/ESP32-Peripheral-Tutorials/Display/RLCD | RLCD 原理、ST7305 腳位、u8g2 範例 |
| 2026-08-04 | Waveshare ESPHome 範例 | https://docs.waveshare.com/ESP32-ESPHome-Tutorials/Example-RLCD-Voice | 完整 GPIO 表、感測器、音訊、電池 ADC |
| 2026-08-04 | 官方電路圖 PDF | https://files.waveshare.com/wiki/ESP32-S3-RLCD-4.2/ESP32-S3-RLCD-4.2-schematic.pdf | 硬體最終依據 |
| 2026-08-04 | ST7305 Datasheet | https://files.waveshare.com/wiki/common/ST_7305_V0_2.pdf | 顯示控制器規格 |
| 2026-08-04 | 官方範例 GitHub | https://github.com/waveshareteam/ESP32-S3-RLCD-4.2 | Arduino / ESP-IDF 範例 |
| 2026-08-04 | ESPHome ST7305 元件 | https://github.com/kylehase/ESPHome-ST7305-RLCD | ESPHome 顯示驅動 |
| 2026-08-04 | Zephyr board doc | https://docs.zephyrproject.org/latest/boards/waveshare/esp32s3_rlcd_4_2/doc/index.html | 第三方 board support 參考 |

## 本機已下載檔案

- `ESP32-S3-RLCD-4.2-schematic.pdf` — 電路圖  
- `ST7305-datasheet.pdf` — ST7305 資料手冊  
- `HARDWARE-SPEC.md` — 彙整規格  
- `PINOUT.md` — GPIO 總表  

## 已知來源差異（備註）

| 項目 | 說明 |
| --- | --- |
| 解析度寫法 | 產品頁寫 300×400；ESPHome 範例以 400×300 橫向使用。兩者為旋轉關係。 |
| 驅動 IC | Waveshare 官方產品頁 / ESPHome 為 **ST7305**；部分 Zephyr 文件寫 ST7306，本專案以官方 ST7305 為準。 |
| Touch | 產品頁未標配觸控；若硬體有觸控需再以電路圖確認。 |

## 更新建議

硬體改版或官方文件更新時：

1. 重新下載 schematic / datasheet  
2. 比對 `HARDWARE-SPEC.md` 與 `PINOUT.md`  
3. 更新本檔擷取日期與差異備註  
