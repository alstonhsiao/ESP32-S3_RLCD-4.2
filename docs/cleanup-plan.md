# 專案清理計畫

更新日期：2026-08-23

本檔追蹤已放棄、重複與尚未完成的工作。清理原則是先保存仍有用途的診斷與硬體資料，再移除已證明無引用或已有正本的內容。使用者已於 2026-08-23 核准依建議執行 C1–C4；C5 依建議保留。

## 已完成的安全整理

- 將現況文件與路由從舊 P0–P7 同步到現行 P0 Combined、P1 Trend、P2–P5 Source Trend。
- 補記 BOOT 短按後閒置 10 秒會額外拉取資料，並說明其 Wi-Fi 與耗電影響。
- `secrets.h` 不存在時改用空值並回退到 WiFiManager，讓乾淨 clone 可以編譯；本機 ignored 私密 symlink 保持不動。
- 修正 flash script 的 `--port` 缺值診斷與 skill 的 exit-code 契約。
- 以不含 `secrets.h` 的暫存副本使用鎖定 FQBN 編譯成功；Flash 使用 38%，全域變數使用 21%。

上述整理沒有改變 `/data`、`POST /trigger`、頁面刷新常數、GPIO、顯示旋轉或電池策略。

## 刪除候選執行結果

| ID | 優先序 | 候選 | 為什麼可考慮刪除 | 刪除前置條件 | 刪除後驗收 | 狀態 |
| --- | --- | --- | --- | --- | --- | --- |
| C1 | P1 | `fmtResetLine()`、`drawSourceCell(..., withReset)` 參數及永不執行的 reset 分支 | 舊 Home 頁移除後，唯一呼叫固定傳 `false`，已無可達使用者 | 再次用 `rg` 確認沒有其他呼叫 | clean-clone 編譯；P0 Combined 實機 smoke test | 已移除；編譯通過，實機驗證仍列 P0 待辦 |
| C2 | P1 | root README 與 sketch README 中重複的裸 `arduino-cli` command blocks | 鎖定流程已有 `.agents/skills/flash-firmware/scripts/flash.sh` 正本；多份 PORT/FQBN 範例容易漂移 | 保留 `docs/firmware-operations.md` 的人類備援與板級設定摘要 | 所有入口仍能導向 script；`bash -n` 與 `--help` 通過 | 已移除並驗證 |
| C3 | P1 | `ui/aiusage-wireframe.html` 中舊 P0 Home、P1 Detail、P3/P3b Pace panels | 這些頁面已由 2026-08-22 的 P0–P5 路由取代，頁碼、刷新與互動敘述均已失真 | 先補現行 P0 Combined／P1 Trend／P2–P5 wireframe，並搬回 Error／Partial 狀態 | 瀏覽器視覺檢查 400×300、1bpp、文字溢位與頁面導航 | 已以現行 P0–P5 替換；桌面與手機視覺檢查通過 |
| C4 | P2 | README 尾端重複的 agent 導航句 | README 頂部已有相同入口，尾端沒有新增資訊 | 確認頂部仍同時連到 Hub 與進度文件 | Markdown 連結檢查 | 已移除並驗證 |
| C5 | P2 | `INVERT_DISPLAY` 待辦 | repo 內只有待辦文字，沒有符號、實作或驗收條件 | 使用者明確決定放棄反顯；若保留，改寫成可驗收工作 | 待辦正本與 README 摘要一致 | 保留，待產品決策 |

未刪除整份 `ui/aiusage-wireframe.html`；Error／Partial 狀態與 1bpp 設計範例已搬入現行 P0–P5 替代稿，僅移除已退役 panels。

## 尚未完成的工作

### P0 — 先證明現行主線可靠

1. 實機驗證 P0–P6：方向為 `U8G2_R1`／400×300、P0 Combined 五列無文字溢位、P1 五線可讀、P2–P6 reset/due 正確。
2. 驗證 `/data` 空 points 時仍會 `POST /trigger`、等待、只重試一次，且所有失敗路徑保有 Serial 診斷。
3. 決定移除舊 Home 後不再突出顯示時間與溫度，是否仍符合 Hub 鎖定的掃讀順序。若要改 Hub 高風險產品方向，必須先取得使用者核准。

### P1 — 清除重複成本與功耗浪費

1. [x] 重做現行 wireframe，並移除 C3 的退役 panels。
2. [x] 執行 C1，移除已證明不可達的程式碼。
3. [x] 修正 `UPD` badge 路徑：頁面與 badge 先合成，再由 `render()` 單次 `sendBuffer()`；3 秒後只重畫一次以清除 badge。
4. [x] 執行 C2，讓 flash script 成為唯一可執行正本。
5. [ ] 完成 HTTPS／約 60 KB JSON、offline／stale 與較長時間運行驗證。

### P2 — 量測、規格與產品待辦

1. 完成至少一個 18650 完整放電週期，記錄 Wi-Fi、輪詢、互動與刷新條件。
2. 決定 SHTC3、KEY 第二操作、反顯、天氣／proxy、MQTT／HA、語音與主 sketch 拆檔的保留或放棄順序。
3. PCF85063 位址與 Touch 備註仍需以本機 schematic／實機掃描確認。`docs/specs/HARDWARE-SPEC.md` 與 `PINOUT.md` 核心語義屬 🟡，修改前必須詢問；不確定時只能追加 `NEED_REVIEW`。
4. `pollData()` 的首次 GET 與 trigger 後重試存在重複流程，但涉及高風險自動恢復路徑；只有在具備空資料與錯誤分支測試後才抽成共用 helper。

## 明確保留

- `firmware/hello_rlcd/`：隔離 ST7305、SPI、方向與燒錄鏈的最小 smoke test。
- `web/`：桌面資料語義、reset／斜率驗證與本機 `/data` proxy，不是韌體的重複副本。
- `.claude/skills/flash-firmware`、`.grok/skills/flash-firmware`：指向 `.agents/skills/flash-firmware` 唯一正本的合法 symlink。
- `docs/specs/` 兩份 PDF：分別是板級 schematic 與 ST7305 控制器資料手冊，角色不同。
- `firmware/aiusage_home/secrets.h`：ignored 的本機私密入口；不得讀取、提交或用 `git clean -fdX` 移除。

## 執行門檻

- C1–C4 已依使用者核准執行；C1 的實機 smoke test 仍併入 P0 主線驗證。
- C5 需要產品方向決策，本次依建議保留。
- 每批刪除後執行 `git diff --check`、引用搜尋及相稱的編譯／視覺驗證；不自動 commit 或 push。
