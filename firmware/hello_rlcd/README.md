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

```bash
FQBN='esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=huge_app,FlashSize=16M,PSRAM=opi'
PORT='/dev/cu.usbmodem11401'   # arduino-cli board list 查看

arduino-cli compile --fqbn "$FQBN" firmware/hello_rlcd
arduino-cli upload -p "$PORT" --fqbn "$FQBN" firmware/hello_rlcd
arduino-cli monitor -p "$PORT" -c baudrate=115200
```

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
