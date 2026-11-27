# TODO

| 預定日期 | 項目 | 說明 |
| --- | --- | --- |
| 2026-12-04 | 移除 `aiusage_home.ino` 的暫時性 `measure:` Serial log | 2026-11-27 為量測 SYNC 耗時加入（`pollTaskFn` 總時間、`ensureWifi` 連線時間、`httpFetchParse` 的 TLS+GET、`logStreamSummary` 的 body 分段計時）。先觀察一週 gzip 版輪詢是否穩定（看 `poll ok`、有無 `gzip` 錯誤），之後刪除 `measure:` 行；`poll: stream scanned ...` 與 `>2 MB WARN` 保留。背景見 `docs/power-and-progress.md` 2026-11-27 段。 |
