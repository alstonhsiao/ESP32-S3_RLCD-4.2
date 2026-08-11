# aiusage_home — P0 / P1 / P2 / P3 / P4–P7

從 `https://aiusage-web.zeabur.app/data` 拉 JSON，在 RLCD 畫 **週剩餘 %**（`100 − used_weekly_pct`）。

## 換頁（BOOT 鍵）

| 操作 | 功能 |
| --- | --- |
| **短按 BOOT** | 循環：`P0 Home` → `P1 Detail` → `P2 Trend` → `P3 Pace` → `P4–P7 四源圖表` → … |
| **長按 BOOT 約 3 秒** | 進入 WiFi 配網（AP `AIUsage-RLCD`） |

BOOT 是板上靠近 USB 的 **BOOT** 側鍵（GPIO0），不是 PWR。

## 頁面

| 頁 | 內容 |
| --- | --- |
| **P0 Home** | 時鐘 + 四源週剩餘 % + bar |
| **P1 Detail** | 表格式 WEEK / 5H / RESET；剩餘 &lt;10% 反白警示 |
| **P2 Trend** | 最近 10 天、最多 128 點剩餘折線（四源線型不同）+ 100/7 輔助虛線 |
| **P3 Pace** | **建議一天用額度**表：`日額% = 週剩% ÷ 剩餘天`；週結束倒數/時鐘、5h 結束倒數/時鐘；節奏 SLOW/OK/FAST；日額最高列反白 |
| **P4–P7 Source Trend** | Claude / Codex / Grok / Ollama 各自一頁；最近 10 天剩餘曲線、實際時間軸、reset 標記、due/reset 理想斜率；提前 reset 時截斷理想線 |

## 功能

| 項目 | 說明 |
| --- | --- |
| 版面 | 對齊 `ui/aiusage-wireframe.html` |
| 來源 | Claude / Codex / Grok / Ollama |
| 刷新 | 資料 **15 分鐘**、自動翻頁 **5 分鐘**（P0–P7 一輪約 40 分鐘）、時鐘 **分鐘變才重畫**；P4–P7 只使用最新 10 天歷史點 |
| Wi‑Fi | **間歇**：只在拉 `/data`（或配網）時連線，結束後 `WIFI_OFF`；底欄 `live`=連線中、`idle`=有資料但 radio 關 |
| P3 語意 | 對齊使用報告「建議一天用」；% 皆剩餘；無 5h 窗顯示 `--` |
| P4–P7 語意 | Y 軸為剩餘 %；理想線由每個 reset 到該週期 due/reset 計算；提前 reset 不強制拉到 0%；reset 後資料不足 6 小時／3 點時實測斜率顯示 `--` |
| 時鐘 | NTP（UTC+8） |
| 電量 | GPIO4 ADC（有 18650 才顯示） |
| WiFi | `secrets.h` 優先；否則 **WiFiManager**（會記住上次配網） |

## 首次配網

螢幕顯示 **WIFI SETUP / AP: AIUsage-RLCD** 時：

1. 手機連 WiFi：`AIUsage-RLCD`（僅 2.4 GHz 可給 ESP 用）
2. 開啟 `http://192.168.4.1`
3. 選家中 2.4G WiFi、輸入密碼、儲存
4. 板子連上後會自動拉 `/data` 並畫 P0

或編輯 `secrets.h`（由 `secrets.h.example` 複製）後重燒：

```cpp
#define WIFI_SSID "你的SSID"
#define WIFI_PASS "你的密碼"
```

`secrets.h` 已在 `.gitignore`，勿提交。

## 編譯燒錄

```bash
FQBN='esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=huge_app,FlashSize=16M,PSRAM=opi'
PORT='/dev/cu.usbmodem101'   # arduino-cli board list 確認

arduino-cli compile --fqbn "$FQBN" --libraries "$HOME/Documents/Arduino/libraries" firmware/aiusage_home
arduino-cli upload -p "$PORT" --fqbn "$FQBN" firmware/aiusage_home
arduino-cli monitor -p "$PORT" -c baudrate=115200
```

依賴：U8g2、ArduinoJson、WiFiManager。

## 預期 Serial

```text
aiusage_home: boot
WiFi OK 192.168.x.x
poll ok: pts=96 C=56 X=88 G=42 O=70
```

## 預期畫面

- 左上時間 + 日期
- 右上 `AI USAGE` / `WEEK REMAIN`
- 2×2：CLAUDE / CODEX / GROK / OLLAMA 大數字 % + bar
- 底欄：`live` + ingest 時間 + 電量
