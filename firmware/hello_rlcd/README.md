# hello_rlcd

最小燒錄驗證：在 4.2" RLCD 顯示 `Hello, RLCD!`，序列埠輸出 heartbeat。

## 需求

- `arduino-cli`
- `esp32:esp32` core ≥ 3.3.0
- 函式庫 **U8g2 ≥ 2.36.19**

```bash
arduino-cli lib install "U8g2"
```

## 板子設定（FQBN）

```text
esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=huge_app,FlashSize=16M,PSRAM=opi
```

| 選項 | 值 |
| --- | --- |
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| Flash Size | 16MB |
| Partition | Huge APP (3MB) |
| PSRAM | OPI PSRAM |

## 編譯 / 燒錄

從 repo 根目錄使用鎖定腳本：

```bash
.agents/skills/flash-firmware/scripts/flash.sh hello_rlcd
```

腳本會沿用上列板級設定，並完成編譯、USB 埠偵測、燒錄及 Serial 檢查。Agent 執行前須先讀 [`flash-firmware` skill](../../.agents/skills/flash-firmware/SKILL.md)；人類手動 `arduino-cli` 備援見 [`docs/firmware-operations.md`](../../docs/firmware-operations.md)。

## 預期結果

**螢幕**（需環境光）：外框、標題、`Hello, RLCD!`、腳位說明、簡單圖形。

**序列埠**：

```text
hello_rlcd: boot
hello_rlcd: display begin OK
hello_rlcd: frame sent — check screen under room light
hello_rlcd: alive … ms
```

## 腳位

| 訊號 | GPIO |
| --- | ---: |
| SCK | 11 |
| MOSI | 12 |
| DC | 5 |
| CS | 40 |
| RST | 41 |
