# aiusage_home — P0–P6

從 `https://aiusage-web.zeabur.app/data` 拉 JSON，在 RLCD 畫 **週剩餘 %**（`100 − used_weekly_pct`）。

## 換頁（BOOT 鍵）

| 操作 | 功能 |
| --- | --- |
| **短按 BOOT** | 循環：`P0 Combined` → `P1 Trend` → `P2–P6 五源圖表` → …；停止操作約 10 秒後拉取新資料 |
| **長按 BOOT 約 3 秒** | 進入 WiFi 配網（AP `AIUsage-RLCD`） |

BOOT 是板上靠近 USB 的 **BOOT** 側鍵（GPIO0），不是 PWR。

## 頁面

| 頁 | 內容 |
| --- | --- |
| **P0 Combined** | 五來源橫列總覽：週剩餘 % + bar、5h、建議一天額度、週 reset、五級節奏 VERY SLOW／SLOW／ON PACE／FAST／VERY FAST |
| **P1 Trend** | 最近 10 天、最多 720 點（20 分鐘一格，較密雲端點自動降取樣）剩餘折線（五源線型不同）+ 100/7 輔助虛線 |
| **P2–P6 Source Trend** | Claude / Chihyi / Alston / Grok / Ollama 各自一頁；最近 10 天剩餘曲線、實際時間軸、reset 標記、due/reset 理想斜率；提前 reset 時截斷理想線 |

## 功能

| 項目 | 說明 |
| --- | --- |
| 版面 | 現行 P0–P6 以主 sketch 為準；`ui/aiusage-wireframe.html` 已同步頁面路由與 Error／Partial 狀態 |
| 來源 | Claude / Chihyi（codex:chihyi）/ Alston（codex:Alston）/ Grok / Ollama |
| 刷新 | 資料 **15 分鐘**、自動翻頁 **5 分鐘**（P0–P6 一輪約 35 分鐘）、reset 倒數／電量在**分鐘變時重畫**；P2–P6 只使用最新 10 天歷史點 |
| Wi‑Fi | **間歇**：只在拉 `/data`（或配網）時連線，結束後 `WIFI_OFF`；底欄 `live`=連線中、`idle`=有資料但 radio 關 |
| 手動更新 | 短按換頁後若 10 秒內沒有再按，額外開 Wi-Fi 拉取一次並短暫顯示 `UPD`；連續翻頁只在最後一次按鍵後觸發一次 |
| P0 語意 | `DAY% = 週剩餘% ÷ 剩餘天`；% 皆為剩餘；無 5h 窗顯示 `--`；節奏對比 `100/7`：內圈 12–18，超過 22 為 VERY SLOW、低於 7 為 VERY FAST |
| P2–P6 語意 | Y 軸為剩餘 %；理想線由每個 reset 到該週期 due/reset 計算；提前 reset 不強制拉到 0%；reset 後資料不足 6 小時／3 點時實測斜率顯示 `--`。節奏以實測斜率對比理想斜率：內圈 ±10% 相對容差、超過 ±20% 為 VERY FAST／VERY SLOW。10 天視窗以 20 分鐘網格降取樣（最多 720 點），確保較早的 reset 也在窗內、理想線可從真實 reset 錨點滿高畫出 |
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

從 repo 根目錄使用鎖定腳本；它會編譯、尋找 USB 埠、燒錄並檢查 Serial：

```bash
.agents/skills/flash-firmware/scripts/flash.sh
```

Agent 執行前須先讀 [`flash-firmware` skill](../../.agents/skills/flash-firmware/SKILL.md)。板級設定、依賴與人類手動 `arduino-cli` 備援見 [`docs/firmware-operations.md`](../../docs/firmware-operations.md)。

## 預期 Serial

```text
aiusage_home: boot
WiFi OK 192.168.x.x
poll ok: pts=96 C=56 H=88 A=22 G=42 O=70
```

## 預期畫面

- P0 標題 `ALL DETAIL`，五列橫排顯示 CLAUDE / CHIHYI / ALSTON / GROK / OLLAMA 的剩餘數值與 reset 資訊
- P1 顯示五來源最近 10 天趨勢；P2–P6 為各來源圖表
- 底欄顯示連線狀態、ingest 時間與電量
