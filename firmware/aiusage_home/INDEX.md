# aiusage_home 路由索引

## 總覽

| 檔案 | 內容形態 | Agent 何時需要 | 讀取順序 |
| --- | --- | --- | --- |
| `aiusage_home.ino` | 大型單檔 Arduino sketch，含資料、網路、按鍵與 P0–P3 繪製 | 任何主儀表行為、穩定度或功耗變更 | 先找對應區段，再讀相關上下文 |
| `README.md` | 人類操作、頁面與配網說明 | 需要確認目前上板行為或驗收步驟 | 先讀摘要與操作表 |
| `secrets.h.example` | 可提交的 Wi-Fi 設定樣板 | 需要寫死 Wi-Fi 或檢查編譯設定 | 只讀欄位，不填入真實密碼 |
| `secrets.h` | 本機 ignored 設定，可能含敏感憑證 | 只在本機編譯／配網故障時按需確認 | 不列入版本控制，不在回報貼出內容 |

## 路由摘要

| 項目 | 一句話說明 | 觸發條件 | 關鍵輸入／輸出 | ⚠️ 注意事項 |
| --- | --- | --- | --- | --- |
| `aiusage_home.ino` | 主程式把衍生 usage JSON 轉成四頁單色 RLCD 儀表。 | 修改 P0–P3、資料解析、Wi-Fi、BOOT、電池或刷新。 | 輸入 `/data`、NTP、GPIO0、GPIO4；輸出 ST7305 畫面與 Serial。 | 先核對 Hub 的固定 GPIO、1bpp buffer、旋轉與功耗規則；不要把 Web UI 直接搬進來。 |
| `README.md` | 定義目前可驗收的換頁、配網、刷新與預期畫面。 | 燒錄前確認操作，或變更後更新人類說明。 | 輸入板級設定與實機觀察；輸出操作步驟與驗收基準。 | 若實作與 README 不同，先確認實際行為，再同步文件；不要用 README 取代 schematic。 |
| `secrets.h.example` | 提供不含真實憑證的覆寫欄位。 | 需要固定 SSID／密碼而不走 NVS 時。 | 輸入使用者自行填寫的本機值；輸出編譯期 macro。 | 真實 `secrets.h` 已 ignored，禁止提交或在 log／回報中暴露。 |
| `secrets.h` | 本機私密設定，不是可移植的專案來源。 | 僅在編譯或配網診斷時確認是否存在。 | 輸入本機憑證；輸出給 sketch 的編譯設定。 | 不要建立索引內容摘要或複製其值；遺失時改用 WiFiManager。 |

## 主要區段路由

- 固定常數與資料模型：檔案開頭的 pins、畫布、輪詢與頁面設定。
- JSON 與網路：`parsePayload`、`pollData`、Wi-Fi／WiFiManager 區段。
- 畫面：`renderHome`、`renderDetail`、`renderTrend`、`renderPace` 與共用繪圖 primitive。
- 互動與生命週期：`handleButton`、`setup`、`loop`。
