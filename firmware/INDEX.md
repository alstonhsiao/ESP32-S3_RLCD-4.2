# firmware 路由索引

## 總覽

| 項目 | 內容形態 | Agent 何時需要 | 下一步 |
| --- | --- | --- | --- |
| `aiusage_home/` | 單一大型 Arduino sketch、操作說明與 Wi-Fi 設定範例 | 修改主儀表、資料拉取、按鍵、頁面或功耗策略時 | 先讀 `aiusage_home/INDEX.md` |
| `hello_rlcd/` | 小型 Arduino 顯示 smoke test 與說明 | 先驗證 ST7305、方向、SPI 腳位或燒錄鏈時 | 讀 `README.md`，再按需讀 `.ino` |

## 路由摘要

| 項目 | 一句話說明 | 觸發條件 | 關鍵輸入／輸出 | ⚠️ 注意事項 |
| --- | --- | --- | --- | --- |
| `aiusage_home/` | 目前主線是四頁 AI 用量儀表。 | 要改 UI、JSON、Wi-Fi、BOOT 或刷新節奏。 | 輸入 aiusage-web 衍生 JSON；輸出 ST7305 1bpp 畫面與 Serial 狀態。 | 先看根目錄 `AGENTS.md` 的腳位、1bpp 與功耗規則；不要讀或提交本機 `secrets.h`。 |
| `hello_rlcd/` | 最小顯示驗證，適合排除主程式以外的硬體問題。 | 螢幕空白、方向錯、SPI 初始化或新板首次燒錄。 | 輸入固定測試圖形；輸出 Hello 畫面與 heartbeat。 | RLCD 需要環境光；測試成功不代表 aiusage 網路路徑已成功。 |

## 共用邊界

- 板級 FQBN、編譯與燒錄流程見 `../docs/firmware-operations.md`。
- 顯示與周邊 GPIO 以 `../docs/specs/INDEX.md` 路由的本機規格為準。
- 任何韌體變更回報都要註明刷新、Wi-Fi、JSON 或電池影響。
