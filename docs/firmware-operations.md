# 韌體與操作

本模組收納已鎖定的 Arduino 技術棧、板級設定、編譯燒錄、配網、復原與目前韌體行為。主 sketch 的檔案路由見 `../firmware/INDEX.md` 與 `../firmware/aiusage_home/INDEX.md`。

## 技術棧與板級設定

| 項目 | 已鎖定內容 |
| --- | --- |
| 框架 | Arduino + U8g2 |
| 顯示驅動 | Waveshare `ST7305_U8g2`，參考官方 demo 與 ClaudeSlate |
| Board/FQBN | `esp32:esp32:esp32s3`、USB CDC On Boot、Huge APP、Flash 16 MB、OPI PSRAM |
| 函式庫 | U8g2（至少 2.36.19）、ArduinoJson、WiFiManager |
| 網路 | `HTTPClient`；WiFiManager 將配網資料記在 NVS，`secrets.h` 可覆寫 |
| 本機工具 | `arduino-cli` 1.5.0；`esp32:esp32` 3.3.7；未採用 ESP-IDF、ESPHome 或 PlatformIO |

## 編譯、燒錄與監看

先用 `arduino-cli board list` 確認目前 PORT，再依目標 sketch 執行：

```bash
FQBN='esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=huge_app,FlashSize=16M,PSRAM=opi'
PORT='/dev/cu.usbmodem11401'
arduino-cli compile --fqbn "$FQBN" --libraries "$HOME/Documents/Arduino/libraries" firmware/aiusage_home
arduino-cli upload -p "$PORT" --fqbn "$FQBN" firmware/aiusage_home
arduino-cli monitor -p "$PORT" -c baudrate=115200
```

Hello smoke test 的目標路徑是 `firmware/hello_rlcd`；主儀表的目標路徑是 `firmware/aiusage_home`。燒錄前後都要核對顯示方向與環境光，RLCD 無背光並不代表畫面沒有輸出。

## aiusage_home 目前行為

- 從 aiusage-web 的 `/data` 取得衍生 JSON，畫 P0 Home、P1 Detail、P2 Trend、P3 Pace。
- 短按 BOOT（GPIO0）循環換頁；長按約 3 秒進入 WiFiManager AP `AIUsage-RLCD`。
- 資料以較長間隔輪詢，畫面通常在分鐘變更時重畫，頁面也會自動輪替；實際常數以 `aiusage_home.ino` 為準。
- 拉資料時才開 Wi-Fi，完成後關閉 radio；這是目前 always-on 電池策略的一部分。
- 沒有資料、部分來源失敗或離線時仍要顯示狀態，不得白屏。

## 配網與操作備忘

| 操作 | 行為 |
| --- | --- |
| 短按 BOOT | P0 → P1 → P2 → P3 循環 |
| 長按 BOOT 約 3 秒 | 顯示 AP 資訊，手機連 `AIUsage-RLCD`，開 `192.168.4.1` 配網 |
| Wi-Fi | 只支援 2.4 GHz；WiFiManager 會記住上次配網 |
| 寫死 Wi-Fi | 由 `secrets.h.example` 複製 `secrets.h`；檔案已 gitignore，勿提交 |
| 看螢幕 | 需要環境光；暗處不可讀是硬體限制 |

## 開機復原

1. 先讀本檔、`../AGENTS.md` 的高風險規則與 `../firmware/INDEX.md`。
2. 插 USB 後執行 `arduino-cli board list`，確認實際 PORT。
3. 若板上不是 aiusage 畫面，重新編譯並燒錄 `../firmware/aiusage_home`。
4. 短按 BOOT 確認 P0 到 P3 都可切換，再決定 UX、感測或穩定度工作。

插拔 Type-C 或 18650 時勿以螢幕受力；不要用超壓供電或暴力拆裝作為復原手段。

## 功耗與驗證邊界

Wi-Fi、HTTPS、較大的 JSON、連續重繪與語音都可能提高耗電；韌體變更回報必須說明是否影響這些項目。電池 ADC 只是粗略百分比，不可當成精密電量或庫侖計。
