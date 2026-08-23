---
name: flash-firmware
description: >
  Compile and upload Waveshare ESP32-S3-RLCD-4.2 Arduino firmware with the
  locked FQBN (CDC On Boot, Huge APP, 16MB flash, OPI PSRAM), then verify
  Serial. Use when the user asks to flash, upload, burn, 燒錄, 燒入,
  更新螢幕, 上傳韌體, 重燒, or runs /flash-firmware.
---

# flash-firmware

本機板子已用 USB 連上時，用腳本燒錄。不要自組 `arduino-cli` FQBN。

## 必做

從 repo 根目錄執行（timeout ≥ 180s）：

```bash
.agents/skills/flash-firmware/scripts/flash.sh
```

| 情境 | 指令 |
| --- | --- |
| 主儀表（預設） | `.agents/skills/flash-firmware/scripts/flash.sh` |
| Hello 驗證 | `.agents/skills/flash-firmware/scripts/flash.sh hello_rlcd` |
| 這次對話已用本腳本編過、只重燒 | `.agents/skills/flash-firmware/scripts/flash.sh --upload-only` |
| 使用者指定埠 | `.agents/skills/flash-firmware/scripts/flash.sh --port /dev/cu.usbmodemXXXX` |
| 跳過 Serial 檢查 | `.agents/skills/flash-firmware/scripts/flash.sh --no-verify` |

## 禁止

- 不要用裸的 `esp32:esp32:esp32s3`（沒有 CDC / Huge APP / 16M / `PSRAM=opi`）。少掉 OPI PSRAM 會讓 `/data` JSON 解析失敗，畫面只剩表格、數字全是 `--`。
- 不要用 `--build-property` 代替 FQBN 選項；`upload` 也不接受 `--build-property`。
- 不要讀、印出或提交 `firmware/**/secrets.h`。
- 燒錄失敗兩次後不要換第三種連線花樣；改請使用者進下載模式。

## 連線失敗

`arduino-cli` 可能報 `No serial data received`。埠也可能在進下載模式時短暫消失，幾秒後以 `/dev/cu.usbmodem*` 回來。

請使用者：

1. 按住 **BOOT**
2. 短按 **RST**
3. 再按住 BOOT 約 2 秒後放開

然後重跑 `.agents/skills/flash-firmware/scripts/flash.sh --upload-only`。同一方法連錯兩次就停，回報完整錯誤。

## 驗收

腳本結束後依 exit code 回報，不要只說「已燒錄」。

| code | 意思 |
| --- | --- |
| 0 | 上傳成功；預設也已看到 Serial 開機字串，使用 `--no-verify` 時則未驗證 Serial |
| 1 | 參數、環境或編譯失敗（例如未知參數、缺 sketch、缺 `arduino-cli`） |
| 2 | 找不到 USB 埠 |
| 3 | 上傳失敗 |
| 4 | 上傳成功但 Serial 沒看到開機字串 |

`aiusage_home` 以 `poll ok:` 代表資料已拉到。只有 `setup done` 而沒有 `poll ok:` 時，說明韌體在跑但雲端資料可能還沒上屏。`hello_rlcd` 看 `display begin OK`。

RLCD 無背光；暗處看不見不代表燒失敗。
