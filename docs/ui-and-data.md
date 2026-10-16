# UI 與資料語義

本模組說明單色 RLCD 的設計意圖、aiusage 頁面資料語義、wireframe 與目前刷新策略。具體版面仍以 `../ui/aiusage-wireframe.html` 與韌體實作互相核對。

## UI 設計原則

- 場景是桌面／牆面 always-on 儀表，優先讓使用者快速掃到時間、溫度、狀態與重要數值。
- 只有黑白 1bpp；用大字、反白條、線框、留白與不同線型建立層次，不假設灰階或背光。
- 400×300 橫向畫布有限，一屏一事；細節由翻頁或 KEY 進入。
- 暗處不可讀是正常情境；文案與互動不能要求使用者在無環境光下確認細節。
- 新增 UI 先在 `../ui/` 建 wireframe 或 mock，再同步到 U8g2。

## aiusage 資料來源與語義

| 項目 | 內容 |
| --- | --- |
| API | `https://aiusage-web.zeabur.app/data`；健康檢查為 `/health` |
| 來源 | `claude`、`codex:chihyi`、`codex:Alston`、`grok`、`ollama` |
| 週數值 | 週剩餘 % = `100 − used_weekly_pct` |
| 5h 數值 | 有滾動窗資料才顯示；沒有時顯示 `--` |
| 節奏標籤 | 五級英文（2026-09-01 起與 usage-web 對齊）：`VERY SLOW / SLOW / ON PACE / FAST / VERY FAST`；退化狀態 `RESET`、`--`／`N/A` 保留 |
| 上游 | 本機 ingest 處理敏感資料，裝置只接收衍生資料 |

不要把彩色 Web UI 原樣搬到 RLCD。資料若繼續膨脹，優先讓 API 只回最新資料與有限歷史點，並保留離線／stale 狀態。

## 頁面路由

| 頁面 | 目的 | 主要輸入／輸出 | 注意事項 |
| --- | --- | --- | --- |
| P0 Combined | 一眼比較五個來源的週、5h、每日建議額度與 reset | 五源週剩餘%、bar、5h、day%、reset、節奏、預測、連線與電池狀態 | bar 表示剩餘比例；日額 = 週剩餘% ÷ 剩餘天；D 五欄儀表，每欄由上往下包含週剩餘、兩行節奏、兩行預測與 5H／D／W，省略 origin 名稱；節奏與網站同用 gap：實際剩餘 − 七天均速理想剩餘，±3pp 內 ON PACE、超過 ±3 至 ±10pp 為 SLOW／FAST，嚴格超過 ±10pp 為 VERY SLOW／VERY FAST |
| P1 Trend | 看近期週剩餘趨勢 | 有限歷史點、五種 1bpp 線型、輔助線 | 多線重疊時優先可讀性，並以 P2–P6 單源頁補足 |
| P2–P6 Source Trend | 分別看 Claude／Chihyi／Alston／Grok／Ollama 最近 10 天週剩餘 | 單一來源剩餘曲線、reset、due/reset 理想線、斜率摘要 | 實際資料限制最近 10 天；提前 reset 時理想線在事件點截斷，reset 後重新由 100% 開始；節奏以實測斜率對比理想斜率：內圈 ±10% 相對容差，超過 ±20% 為 VERY FAST／VERY SLOW；斜率窗不足 6 小時顯示 `RESET`／`--` |
| Error／Partial | 讓失敗狀態仍可讀 | 無資料、部分來源、重試與連線狀態 | 不得以白屏掩蓋 HTTP、Wi-Fi 或資料問題 |

## Wireframe 與參考實作

- 人類預覽入口：`../ui/README.md`；HTML wireframe：`../ui/aiusage-wireframe.html`。該 wireframe 已對齊現行 P0 Combined／P1 Trend／P2–P6，並保留 Error／Partial 狀態供失敗路徑驗收。
- `../ui/` 的版面應與 `../firmware/aiusage_home/aiusage_home.ino` 的 P0–P6 對照，不要只改其中一邊。
- ClaudeSlate 是同一塊硬體的參考專案，可借鏡 400×300 單色資訊設計、進度條、折線、BOOT 翻頁、captive portal、離線狀態與軟體反顯；需求不符時以本 repo 規則為準。

## 刷新與功耗邊界

- 目前實作採長間隔拉資料、分鐘級時鐘刷新與較慢的自動翻頁；精確週期以主 sketch 常數為準。
- 互動時可即時更新，但靜態畫面不應無意義全屏重繪。
- Wi-Fi 連線、HTTPS、語音與高 FPS 動畫都要在設計或回報中標示為高耗電模式。

## ALL DETAIL 預測（2026-10-16）

預測以最新成功快照的 `ts_epoch` 為基準，假設七天週期與後續維持本週累積平均消耗：`d = (reset − observed) / 86400`、`e = 7 − d`、`v = (100 − R) / e`、`P = R − v × d`。`R` 為週剩餘百分比。`P < 0` 時顯示 `EARLY x.xD`，提前天數為 `d − R / v`；否則顯示 `LEFT xx%`。零消耗可預測剩 100%，避免除零；額度用完直接顯示 `USED UP`。

週期未滿 12 小時先顯示 `WARMUP`；資料超過 6 小時、未來快照、缺失時間、已過期 reset 或失敗來源不顯示數字預測。分鐘重畫不會讓同一筆資料的預測餘額自行改善。

USD 為未使用額度的訂閱費分攤估值，並非退款或可兌現餘額。為與網站一致，採「月費 ÷ 4 × 顯示的整數預測剩餘比例」。預設月費 CLAUDE 20、CHIHYI 21、ALSTON 21、GROK 30、OLLAMA 20 USD，集中於韌體常數；實際訂閱不同時應調整。

顯示維持 `U8G2_R1`、400×300、1bpp，需要環境光。新增預測只做本機計算；每 15 分鐘抓資料、分鐘級重畫、每 5 分鐘翻頁與互動延遲更新策略維持原設定，沒有新增 Wi-Fi 連線或刷新。

## ALL DETAIL D 五欄儀表（2026-10-16）

使用者選定 `ui/all-detail-layout-options.html` 的 D 方案。P0 採五個等距 76px 欄位（CLAUDE／CHIHYI／ALSTON／GROK／OLLAMA），以垂直分隔線區分來源；從上到下為名稱、週剩餘大字與 bar、分行節奏、預測、5H／D／W。`LEFT xx%` 與 USD 分開兩行，`EARLY` 與提前天數分開兩行；失敗來源保留名稱，數值顯示 `--`，預測區呈現 MISSING／SRC FAIL。D 最大值的 `*` 仍保留，連線來源名稱省略。標頭顯示本地時間，UPD 提示出現時預留空間；底欄維持既有狀態與電量。

畫布維持 R1 400×300、1bpp、無背光；內容止於 y=252，底欄分隔線 y=258。實際 U8g2 字型輸出存於 `ui/all-detail-d.svg` 與 `ui/all-detail-d-partial.svg`，供現行 wireframe 載入；固定快照只用於預覽。此次只調整顯示，預測公式、GPIO、15 分鐘抓取、分鐘刷新、5 分鐘翻頁與 Wi-Fi 關閉策略均不變。
