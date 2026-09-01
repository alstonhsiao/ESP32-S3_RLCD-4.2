/*
 * aiusage_home — AI week-remain dashboard on Waveshare ESP32-S3-RLCD-4.2
 *
 * Data: GET https://aiusage-web.zeabur.app/data
 *   Cloud SQLite (persistent volume) fed by local usage-history.db via upsert.
 *   If /data returns 0 points, firmware auto-triggers POST /trigger to kick
 *   KM → notify_all → sync_usage_web, then re-fetches.
 *   The payload is scanned as a byte stream and never buffered whole: it has
 *   outgrown internal RAM (686 KB by 2026-08-30), which crashed the WiFi PHY
 *   timer alloc with ESP_ERR_NO_MEM. Only the last point + the 10d trend grid
 *   are kept.
 * remain% = 100 - used_weekly_pct  (same as aiusage-web / Telegram)
 *
 * Pages (auto every 5m; short-press BOOT advances + resets timer):
 *   P0 Combined — 5 horizontal rows: remain% + 5h + day% + reset per source
 *   P1 Trend  — last 10d remain% polylines (5 series, distinct 1bpp styles)
 *   P2-P6    — one 10d remain% chart per source, reset-aware ideal slope
 * Power: poll cloud every 15m then WiFi OFF; redraw only when minute changes
 * Long-press BOOT 3s → WiFi setup portal (AIUsage-RLCD)
 *
 * Board: ESP32S3 Dev Module | CDC On Boot | Huge APP | Flash 16MB | OPI PSRAM
 * Display: ST7305 SPI SCK=11 MOSI=12 DC=5 CS=40 RST=41  (U8G2_R1 = 400x300)
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <WiFiManager.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#define WIFI_SSID ""
#define WIFI_PASS ""
#endif

// ---- pins ----
#define RLCD_SCK  11
#define RLCD_MOSI 12
#define RLCD_DC   5
#define RLCD_CS   40
#define RLCD_RST  41
#define BAT_ADC_PIN 4
#define BTN_BOOT  0

#define W 400
#define H 300
#define N_SOURCES 5
#define PAGE_COUNT 7
#define CHART_PAGE_BASE 2
// P0 pace thresholds vs the ~100/7 (≈14.3) daily ideal: inner ring keeps the
// historic 12/18 band; outer ring ≈ ±7pp (≈1.5 days of quota off pace).
#define PACE_DAILY_FAST 12.0f
#define PACE_DAILY_SLOW 18.0f
#define PACE_DAILY_VFAST 7.0f
#define PACE_DAILY_VSLOW 22.0f
// Trend buffer: 20-min grid over the 10d window (720 slots). The stream
// scanner downsamples denser cloud points by keeping the last point of each
// slot, so resets older than ~2 days stay visible as R anchors.
#define TREND_N 720
#define TREND_GRID_SEC (20L * 60L)
#define TREND_WINDOW_SEC (10L * 86400L)
#define MIN_SLOPE_WINDOW_SEC (6L * 3600L)
// Pace ring 1 (inner, historic ±10% relative) and ring 2 (severe, ±20%).
#define SLOPE_TOLERANCE 0.10f
#define SLOPE_TOLERANCE_SEVERE 0.20f
// Reset markers are sparse (a handful per week); no need to scale with TREND_N.
#define TREND_EVENT_MAX 32

#include "usage_scan.h"

// Data source: cloud SQLite (persistent volume on Zeabur), fed by local SQLite upsert.
// GET /data reads from cloud DB; POST /trigger kicks KM to query AI quota + sync.
static const char* DATA_URL = "https://aiusage-web.zeabur.app/data";
static const char* TRIGGER_URL = "https://aiusage-web.zeabur.app/trigger";
static const uint32_t POLL_MS = 900000;      // fetch cloud data every 15 minutes
static const uint32_t TRIGGER_RETRY_MS = 8000; // wait for KM→sync round-trip before re-fetch
static const uint32_t RENDER_MS = 60000;     // fallback redraw if NTP not ready
static const uint32_t AUTO_PAGE_MS = 300000; // auto flip every 5 minutes
static const uint32_t LONG_PRESS_MS = 3000;
static const uint32_t WIFI_CONNECT_MS = 20000;

U8G2_ST7305_300X400_F_4W_HW_SPI u8g2(U8G2_R1, RLCD_CS, RLCD_DC, RLCD_RST);

enum class LinkState : uint8_t {
  Booting, WifiSetup, Connecting, Online, Offline, HttpError, ParseError, Empty
};

struct SourceUi {
  const char* name;
  char origin[16];
  bool ok;
  bool present;
  float remainWeek;   // 0..100, or -1
  float remain5h;     // 0..100, or -1 if N/A
  long resetWeek;     // epoch, 0 if none
  long reset5h;
};

struct UsageSnapshot {
  bool valid = false;
  int pointCount = 0;
  char ingestShort[20] = "";
  SourceUi src[N_SOURCES] = {
    {"CLAUDE", "", false, false, -1, -1, 0, 0},
    {"CHIHYI", "", false, false, -1, -1, 0, 0},
    {"ALSTON", "", false, false, -1, -1, 0, 0},
    {"GROK",   "", false, false, -1, -1, 0, 0},
    {"OLLAMA", "", false, false, -1, -1, 0, 0},
  };
  // trend: remain% 0..100, or -1 missing; chronological oldest→newest
  int8_t trend[N_SOURCES][TREND_N];
  long trendTs[TREND_N];
  long trendResetWeek[N_SOURCES][TREND_N];
  int trendN = 0;
  char trendStartLbl[8] = "";
  char trendEndLbl[8] = "";
};

static UsageSnapshot gSnap;
static LinkState gLink = LinkState::Booting;
static int gPage = 0;
static int gBat = -1;
static uint32_t gLastPoll = 0;
static uint32_t gLastRender = 0;
static uint32_t gLastPageChange = 0;  // auto-page timer (reset on BOOT short-press)
static char gLastErr[48] = "";
static int gLastBtn = HIGH;
static uint32_t gBtnDownMs = 0;
static bool gLongPressFired = false;
static int gLastDrawnMinuteKey = -1;  // hour*60+min; skip full redraw if unchanged
static bool gShowUpdateBadge = false;      // true → draw "↻" badge after render
static uint32_t gUpdateBadgeUntil = 0;     // millis() when badge expires
static uint32_t gLastManualFlip = 0;       // millis() of last BOOT short-press; 0 = none pending

static void render();  // forward
static void drawUpdateBadge();
static bool haveLocalTime(struct tm* t);
static bool ensureWifi(uint32_t timeoutMs = WIFI_CONNECT_MS);
static void radioOff();

struct TrendResetEvent {
  long time;
  long due;
  int beforeRemain;
  int afterRemain;
  bool confirmed;
};

static TrendResetEvent gTrendEvents[TREND_EVENT_MAX];
static int gTrendEventN = 0;

static void noteDrawnMinute() {
  struct tm t;
  if (haveLocalTime(&t)) gLastDrawnMinuteKey = t.tm_hour * 60 + t.tm_min;
}

static bool minuteChanged() {
  struct tm t;
  if (!haveLocalTime(&t)) return false;
  int key = t.tm_hour * 60 + t.tm_min;
  if (key == gLastDrawnMinuteKey) return false;
  gLastDrawnMinuteKey = key;
  return true;
}

static void advancePage(uint32_t now, const char* reason) {
  gPage = (gPage + 1) % PAGE_COUNT;
  gLastPageChange = now;
  gLastRender = now;
  Serial.printf("page → P%d (%s)\n", gPage, reason);
  render();
  noteDrawnMinute();
}

// ---- helpers ----
static void strRight(int rx, int y, const char* s) {
  u8g2.drawStr(rx - u8g2.getStrWidth(s), y, s);
}

static void strCenter(int cx, int y, const char* s) {
  u8g2.drawStr(cx - u8g2.getStrWidth(s) / 2, y, s);
}

static void drawBar(int x, int y, int w, int h, float frac01) {
  u8g2.drawFrame(x, y, w, h);
  float f = frac01;
  if (f < 0) f = 0;
  if (f > 1) f = 1;
  int fill = (int)((w - 4) * f + 0.5f);
  if (fill > 0) u8g2.drawBox(x + 2, y + 2, fill, h - 4);
}

static int readBatteryPct() {
  long sum = 0;
  int n = 0;
  for (int i = 0; i < 8; i++) {
    int mv = analogReadMilliVolts(BAT_ADC_PIN);
    if (mv > 0) { sum += mv; n++; }
    delay(2);
  }
  if (n == 0) return gBat;
  int mv = (int)(sum / n) * 3;
  if (mv < 2500 || mv > 4600) return gBat;
  float v = mv / 1000.0f;
  int pct = (v < 3.0f) ? 0 : (v > 4.12f ? 100 : (int)round((v - 3.0f) / 1.12f * 100.0f));
  if (gBat >= 0 && abs(pct - gBat) < 2) return gBat;
  return pct;
}

static void drawBatteryRight(int rx, int yTop, int pct) {
  if (pct < 0) return;
  char b[8];
  snprintf(b, sizeof(b), "%d%%", pct);
  u8g2.setFont(u8g2_font_6x13_tf);
  int total = 24 + 4 + u8g2.getStrWidth(b);
  int bx = rx - total;
  u8g2.drawFrame(bx, yTop, 22, 11);
  u8g2.drawBox(bx + 22, yTop + 3, 2, 5);
  int fw = (int)round(pct / 100.0 * 18);
  if (fw > 0) u8g2.drawBox(bx + 2, yTop + 2, fw, 7);
  u8g2.drawStr(bx + 28, yTop + 10, b);
}

static bool haveLocalTime(struct tm* t) {
  if (!getLocalTime(t, 50)) return false;
  return t->tm_year > 120;
}

static void fmtReset(long resetUnix, char* out, size_t n) {
  if (resetUnix <= 0) {
    snprintf(out, n, "--");
    return;
  }
  time_t now = time(nullptr);
  if (now < 100000) {
    snprintf(out, n, "--");
    return;
  }
  long s = (long)(resetUnix - (long)now);
  if (s <= 0) {
    snprintf(out, n, "now");
    return;
  }
  if (s < 3600) snprintf(out, n, "%ldm", s / 60);
  else if (s < 86400) snprintf(out, n, "%ldh %ldm", s / 3600, (s % 3600) / 60);
  else snprintf(out, n, "%ldd %ldh", s / 86400, (s % 86400) / 3600);
}

// Days until reset (fractional). -1 if unknown.
static float daysUntil(long resetUnix) {
  if (resetUnix <= 0) return -1.0f;
  time_t now = time(nullptr);
  if (now < 100000) return -1.0f;
  long s = resetUnix - (long)now;
  if (s <= 0) return 0.0f;
  return (float)s / 86400.0f;
}

// Suggested 24h spend % = week remain% / days left. -1 if N/A.
static float dailyBudgetPct(const SourceUi& s) {
  if (!s.present || !s.ok || s.remainWeek < 0.0f || s.resetWeek <= 0) return -1.0f;
  float d = daysUntil(s.resetWeek);
  if (d < 0.0f) return -1.0f;
  if (d < 0.05f) d = 0.05f;  // avoid explode near reset
  return s.remainWeek / d;
}

// Pace vs ~100/7 daily (ASCII only — U8g2 Latin fonts).
// SLOW = under-using (high daily budget left), FAST = over-using.
// Five levels (aligned with usage-web 2026-09-01): inner ring keeps the
// historic ±4pp band, outer ring ~1.5 days of quota off the weekly ideal.
static const char* paceLabel(float daily) {
  if (daily < 0.0f) return "--";
  if (daily > PACE_DAILY_VSLOW) return "VERY SLOW";
  if (daily > PACE_DAILY_SLOW) return "SLOW";
  if (daily < PACE_DAILY_VFAST) return "VERY FAST";
  if (daily < PACE_DAILY_FAST) return "FAST";
  return "ON PACE";
}

static void shortOrigin(const char* origin, char* out, size_t n) {
  if (!origin || !origin[0]) { out[0] = 0; return; }
  if (strstr(origin, "oauth")) snprintf(out, n, "oauth");
  else if (strstr(origin, "app-server")) snprintf(out, n, "app-server");
  else if (strstr(origin, "billing")) snprintf(out, n, "billing");
  else if (strstr(origin, "cookie")) snprintf(out, n, "cookie");
  else if (strstr(origin, "cli")) snprintf(out, n, "cli");
  else {
    snprintf(out, n, "%s", origin);
    if (strlen(out) > 12) out[12] = 0;
  }
}

static void shortIngest(const char* iso, char* out, size_t n) {
  if (!iso || !iso[0]) { snprintf(out, n, "--"); return; }
  const char* t = strchr(iso, 'T');
  if (t && strlen(t) >= 6) {
    snprintf(out, n, "%.5s", t + 1);
    return;
  }
  snprintf(out, n, "%.16s", iso);
}

static void labelFromIsoOrEpoch(const char* iso, long epoch, char* out, size_t n) {
  if (iso && iso[0] && strlen(iso) >= 10) {
    snprintf(out, n, "%.2s/%.2s", iso + 5, iso + 8);
    return;
  }
  if (epoch > 100000) {
    time_t t = (time_t)epoch;
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(out, n, "%02d/%02d", tm.tm_mon + 1, tm.tm_mday);
    return;
  }
  snprintf(out, n, "--");
}

static float remainFromUsed(float used) {
  float rem = 100.0f - used;
  if (rem < 0) rem = 0;
  if (rem > 100) rem = 100;
  return rem;
}

static const char* linkLabel(LinkState s) {
  switch (s) {
    case LinkState::Booting: return "boot";
    case LinkState::WifiSetup: return "setup";
    case LinkState::Connecting: return "wifi...";
    case LinkState::Online:
      // After poll we turn radio off; still show last-good data as idle.
      return (WiFi.status() == WL_CONNECTED) ? "live" : "idle";
    case LinkState::Offline: return "offline";
    case LinkState::HttpError: return "http err";
    case LinkState::ParseError: return "parse";
    case LinkState::Empty: return "no data";
    default: return "?";
  }
}

// ---- streaming scan of /data ----
// The cloud payload grows unboundedly (full history; 686 KB and rising as of
// 2026-08-30). http.getString() materialized it in internal RAM, exhausted the
// heap, and crashed the WiFi PHY timer alloc (ESP_ERR_NO_MEM in
// phy_track_pll_init) → reboot loop with the screen stuck on "no data". We now
// feed bytes from the HTTP stream through a scanner that keeps only what the
// UI needs: the last point's per-source values and the 10d trend buffer.
// Host-verified against the live payload: all 1039 points, every field
// identical to the previous full-JSON parse.
static const char* SRC_KEYS[N_SOURCES] = {"claude", "codex:chihyi", "codex:Alston", "grok", "ollama"};

static TrendSlot gScanRing[TREND_N];
static int gScanRingHead = 0;
static int gScanRingN = 0;

static void scanRingClear() {
  gScanRingHead = 0;
  gScanRingN = 0;
}

static void scanRingPush(const TrendSlot& s) {
  if (gScanRingN < TREND_N) {
    gScanRing[(gScanRingHead + gScanRingN) % TREND_N] = s;
    gScanRingN++;
  } else {
    gScanRing[gScanRingHead] = s;
    gScanRingHead = (gScanRingHead + 1) % TREND_N;
  }
}

static bool sourceScanOk(const ScanPoint& p, int i) {
  if (p.hasError[i]) return false;
  if (p.hasOk[i]) return !p.okFalse[i];
  return p.hasWeekly[i] || p.has5h[i];
}

static void scanPushSlot(const ScanPoint& p) {
  if (!p.hasEpoch || p.epoch <= 100000) return;
  TrendSlot s;
  s.epoch = p.epoch;
  s.md[0] = 0;
  if (p.iso[0] && strlen(p.iso) >= 10)
    snprintf(s.md, sizeof(s.md), "%.2s/%.2s", p.iso + 5, p.iso + 8);
  for (int i = 0; i < N_SOURCES; i++) {
    s.remain[i] = -1;
    s.resetWeek[i] = 0;
    if (!p.present[i] || !sourceScanOk(p, i) || !p.hasWeekly[i]) continue;
    s.remain[i] = (int8_t)(remainFromUsed(p.weekly[i]) + 0.5f);
    s.resetWeek[i] = p.resetWeek[i];
  }
  scanRingPush(s);
}

static void scanApplyPoint(ScanState& st) {
  st.last = st.pt;
  st.pointCount++;
  if (!st.pt.hasEpoch || st.pt.epoch <= 100000) return;
  long slot = st.pt.epoch / TREND_GRID_SEC;
  if (st.slotHas && slot == st.slotNo) {
    st.slotPt = st.pt;
    return;
  }
  if (st.slotHas) scanPushSlot(st.slotPt);
  st.slotHas = true;
  st.slotNo = slot;
  st.slotPt = st.pt;
}

static void scanApplyStringValue(ScanState& st, const char* s) {
  if (st.inPoint && st.depth == st.pointDepth && !strcmp(st.valueKey, "ts")) {
    strncpy(st.pt.iso, s, sizeof(st.pt.iso) - 1);
    st.pt.iso[sizeof(st.pt.iso) - 1] = 0;
  } else if (st.depth == 1 && !st.seenIngest
             && (!strcmp(st.valueKey, "last_ingest_at") || !strcmp(st.valueKey, "exported_at"))) {
    shortIngest(s, gSnap.ingestShort, sizeof(gSnap.ingestShort));
    st.seenIngest = true;
  } else if (st.sourcesDepth >= 0 && st.depth == st.sourcesDepth + 1 && st.srcIdx >= 0) {
    if (!strcmp(st.valueKey, "origin")) {
      shortOrigin(s, st.pt.origin[st.srcIdx], sizeof(st.pt.origin[0]));
    } else if (!strcmp(st.valueKey, "error")) {
      st.pt.hasError[st.srcIdx] = true;
    }
  }
}

static void scanApplyNumber(ScanState& st, double v) {
  if (st.inPoint && st.depth == st.pointDepth && !strcmp(st.valueKey, "ts_epoch")) {
    st.pt.epoch = (long)v;
    st.pt.hasEpoch = true;
  } else if (st.sourcesDepth >= 0 && st.depth == st.sourcesDepth + 1 && st.srcIdx >= 0) {
    if (!strcmp(st.valueKey, "used_weekly_pct")) {
      st.pt.weekly[st.srcIdx] = (float)v;
      st.pt.hasWeekly[st.srcIdx] = true;
    } else if (!strcmp(st.valueKey, "used_5h_pct")) {
      st.pt.used5h[st.srcIdx] = (float)v;
      st.pt.has5h[st.srcIdx] = true;
    } else if (!strcmp(st.valueKey, "resets_weekly_at")) {
      st.pt.resetWeek[st.srcIdx] = (long)v;
    } else if (!strcmp(st.valueKey, "resets_5h_at")) {
      st.pt.reset5h[st.srcIdx] = (long)v;
    }
  }
}

static void scanApplyLiteral(ScanState& st, const char* tok) {
  if (st.sourcesDepth >= 0 && st.depth == st.sourcesDepth + 1 && st.srcIdx >= 0) {
    if (!strcmp(st.valueKey, "ok")) {
      st.pt.hasOk[st.srcIdx] = true;
      st.pt.okFalse[st.srcIdx] = (strcmp(tok, "false") == 0);
    }
  }
}

static void scanOnColon(ScanState& st) {
  strncpy(st.valueKey, st.pendingKey, sizeof(st.valueKey) - 1);
  st.valueKey[sizeof(st.valueKey) - 1] = 0;
  st.expectKey = false;
  if (st.sourcesDepth >= 0 && st.depth == st.sourcesDepth) {
    st.srcIdx = -1;
    for (int i = 0; i < N_SOURCES; i++) {
      if (!strcmp(st.pendingKey, SRC_KEYS[i])) { st.srcIdx = i; break; }
    }
    if (st.srcIdx >= 0) st.pt.present[st.srcIdx] = true;
  }
}

static void scanFeed(ScanState& st, char c) {
  switch (st.mode) {
    case 0: {
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return;
      if (c == '"') { st.mode = 1; st.bufLen = 0; return; }
      if (c == '{') {
        if (st.depth < 31) {
          st.depth++;
          st.nest[st.depth] = 1;
        } else {
          st.depth++;
        }
        st.expectKey = true;
        if (st.pointsDepth >= 0 && st.depth == st.pointsDepth + 1 && !st.inPoint) {
          st.inPoint = true;
          st.pointDepth = st.depth;
          st.pt = ScanPoint();
        } else if (st.inPoint && !strcmp(st.valueKey, "sources")
                   && st.depth == st.pointDepth + 1) {
          st.sourcesDepth = st.depth;
        }
        return;
      }
      if (c == '}') {
        if (st.inPoint && st.depth == st.pointDepth) {
          scanApplyPoint(st);
          st.inPoint = false;
          st.pointDepth = -1;
        } else if (st.sourcesDepth >= 0 && st.depth == st.sourcesDepth) {
          st.sourcesDepth = -1;
          st.srcIdx = -1;
        } else if (st.sourcesDepth >= 0 && st.depth == st.sourcesDepth + 1) {
          st.srcIdx = -1;
        }
        if (st.depth > 0) st.depth--;
        st.expectKey = (st.depth > 0 && st.depth < 32 && st.nest[st.depth] == 1);
        return;
      }
      if (c == '[') {
        if (st.depth < 31) {
          st.depth++;
          st.nest[st.depth] = 0;
        } else {
          st.depth++;
        }
        st.expectKey = false;
        if (st.pointsDepth < 0 && !strcmp(st.valueKey, "points"))
          st.pointsDepth = st.depth;
        return;
      }
      if (c == ']') {
        if (st.depth > 0) st.depth--;
        if (st.pointsDepth >= 0 && st.depth < st.pointsDepth) st.pointsDepth = -1;
        st.expectKey = (st.depth > 0 && st.depth < 32 && st.nest[st.depth] == 1);
        return;
      }
      if (c == ':') { scanOnColon(st); return; }
      if (c == ',') {
        st.valueKey[0] = 0;
        st.expectKey = (st.depth > 0 && st.depth < 32 && st.nest[st.depth] == 1);
        return;
      }
      if (c == '-' || (c >= '0' && c <= '9')) {
        st.mode = 4;
        st.bufLen = 0;
        st.buf[st.bufLen++] = c;
        return;
      }
      if (c >= 'a' && c <= 'z') {
        st.mode = 5;
        st.bufLen = 0;
        st.buf[st.bufLen++] = c;
        return;
      }
      return;
    }
    case 1: {
      if (c == '\\') { st.mode = 2; return; }
      if (c == '"') {
        st.mode = 0;
        st.buf[st.bufLen] = 0;
        if (st.expectKey) {
          strncpy(st.pendingKey, st.buf, sizeof(st.pendingKey) - 1);
          st.pendingKey[sizeof(st.pendingKey) - 1] = 0;
        } else {
          scanApplyStringValue(st, st.buf);
        }
        return;
      }
      if (st.bufLen < sizeof(st.buf) - 1) st.buf[st.bufLen++] = c;
      return;
    }
    case 2: {
      if (c == 'u') { st.mode = 3; st.uniN = 0; return; }
      st.mode = 1;
      char m = c;
      if (c == 'n') m = '\n';
      else if (c == 't') m = '\t';
      else if (c == 'r') m = '\r';
      if (st.bufLen < sizeof(st.buf) - 1) st.buf[st.bufLen++] = m;
      return;
    }
    case 3: {
      if (st.uniN < 4) st.uniHex[st.uniN++] = c;
      if (st.uniN == 4) {
        st.uniHex[4] = 0;
        long cp = strtol(st.uniHex, nullptr, 16);
        st.mode = 1;
        if (cp > 0 && cp < 128 && st.bufLen < sizeof(st.buf) - 1)
          st.buf[st.bufLen++] = (char)cp;
      }
      return;
    }
    case 4: {
      if (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        st.mode = 0;
        st.buf[st.bufLen] = 0;
        scanApplyNumber(st, strtod(st.buf, nullptr));
        scanFeed(st, c);
        return;
      }
      if (st.bufLen < sizeof(st.buf) - 1) st.buf[st.bufLen++] = c;
      return;
    }
    case 5: {
      if (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        st.mode = 0;
        st.buf[st.bufLen] = 0;
        scanApplyLiteral(st, st.buf);
        scanFeed(st, c);
        return;
      }
      if (st.bufLen < sizeof(st.buf) - 1) st.buf[st.bufLen++] = c;
      return;
    }
  }
}

static void scanFinish(ScanState& st) {
  if (st.slotHas) scanPushSlot(st.slotPt);
  gSnap.pointCount = (int)st.pointCount;
  long lastEp = st.last.epoch;
  long cutoff = (lastEp > 100000) ? lastEp - TREND_WINDOW_SEC : 0;
  int ti = 0;
  for (int i = 0; i < gScanRingN && ti < TREND_N; i++) {
    const TrendSlot& s = gScanRing[(gScanRingHead + i) % TREND_N];
    if (s.epoch < cutoff) continue;
    gSnap.trendTs[ti] = s.epoch;
    if (ti == 0) {
      if (s.md[0]) snprintf(gSnap.trendStartLbl, sizeof(gSnap.trendStartLbl), "%s", s.md);
      else labelFromIsoOrEpoch("", s.epoch, gSnap.trendStartLbl, sizeof(gSnap.trendStartLbl));
    }
    if (s.md[0]) snprintf(gSnap.trendEndLbl, sizeof(gSnap.trendEndLbl), "%s", s.md);
    else labelFromIsoOrEpoch("", s.epoch, gSnap.trendEndLbl, sizeof(gSnap.trendEndLbl));
    for (int k = 0; k < N_SOURCES; k++) {
      gSnap.trend[k][ti] = s.remain[k];
      gSnap.trendResetWeek[k][ti] = s.resetWeek[k];
    }
    ti++;
  }
  gSnap.trendN = ti;

  for (int i = 0; i < N_SOURCES; i++) {
    if (!st.last.present[i]) continue;
    SourceUi& dst = gSnap.src[i];
    dst.present = true;
    dst.ok = sourceScanOk(st.last, i);
    strncpy(dst.origin, st.last.origin[i], sizeof(dst.origin) - 1);
    dst.origin[sizeof(dst.origin) - 1] = 0;
    dst.remainWeek = st.last.hasWeekly[i] ? remainFromUsed(st.last.weekly[i]) : -1.0f;
    dst.remain5h = st.last.has5h[i] ? remainFromUsed(st.last.used5h[i]) : -1.0f;
    dst.resetWeek = st.last.resetWeek[i];
    dst.reset5h = st.last.reset5h[i];
  }
  gSnap.valid = (st.pointCount > 0);
}

static void snapReset() {
  gSnap.valid = false;
  gSnap.pointCount = 0;
  gSnap.trendN = 0;
  gSnap.ingestShort[0] = 0;
  for (int i = 0; i < N_SOURCES; i++) {
    gSnap.src[i].ok = false;
    gSnap.src[i].present = false;
    gSnap.src[i].remainWeek = -1;
    gSnap.src[i].remain5h = -1;
    gSnap.src[i].resetWeek = 0;
    gSnap.src[i].reset5h = 0;
    gSnap.src[i].origin[0] = 0;
    for (int j = 0; j < TREND_N; j++) {
      gSnap.trend[i][j] = -1;
      gSnap.trendResetWeek[i][j] = 0;
    }
  }
  for (int j = 0; j < TREND_N; j++) gSnap.trendTs[j] = 0;
  gSnap.trendStartLbl[0] = 0;
  gSnap.trendEndLbl[0] = 0;
}

static bool parseStream(NetworkClient& stream, int contentLen) {
  snapReset();
  scanRingClear();
  static ScanState st;
  st = ScanState();

  uint8_t chunk[512];
  size_t total = 0;
  uint32_t lastData = millis();
  const uint32_t stallMs = 15000;
  while (contentLen < 0 || (int)total < contentLen) {
    int avail = stream.available();
    if (avail <= 0) {
      if (!stream.connected()) break;
      if (millis() - lastData > stallMs) break;
      delay(2);
      continue;
    }
    size_t want = sizeof(chunk);
    if ((size_t)avail < want) want = (size_t)avail;
    if (contentLen >= 0) {
      size_t left = (size_t)contentLen - total;
      if (left < want) want = left;
    }
    int rd = stream.readBytes((char*)chunk, want);
    if (rd <= 0) {
      if (!stream.connected()) break;
      if (millis() - lastData > stallMs) break;
      continue;
    }
    lastData = millis();
    for (int k = 0; k < rd; k++) scanFeed(st, (char)chunk[k]);
    total += (size_t)rd;
  }
  scanFinish(st);
  Serial.printf("poll: stream scanned %u bytes, pts=%d trend=%d\n",
                (unsigned)total, gSnap.pointCount, gSnap.trendN);
  if (total == 0) {
    snprintf(gLastErr, sizeof(gLastErr), "empty body");
    return false;
  }
  return true;
}

// ---- fetch ----
static bool triggerUpstreamRefresh() {
  WiFiClientSecure tClient;
  tClient.setInsecure();
  tClient.setTimeout(12);
  HTTPClient thttp;
  thttp.setConnectTimeout(10000);
  thttp.setTimeout(12000);
  if (!thttp.begin(tClient, TRIGGER_URL)) {
    Serial.println("trigger: begin fail");
    return false;
  }
  int code = thttp.POST("");
  String body = thttp.getString();
  thttp.end();
  Serial.printf("trigger: HTTP %d body=%s\n", code, body.c_str());
  return code == 200;
}

static bool httpFetchParse(const char* tag) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(12);
  HTTPClient http;
  http.setConnectTimeout(15000);
  http.setTimeout(30000);
  http.useHTTP10(true);
  if (!http.begin(client, DATA_URL)) {
    gLink = LinkState::HttpError;
    snprintf(gLastErr, sizeof(gLastErr), "begin fail");
    Serial.printf("%s: begin fail\n", tag);
    return false;
  }
  int code = http.GET();
  if (code != 200) {
    snprintf(gLastErr, sizeof(gLastErr), "HTTP %d", code);
    gLink = LinkState::HttpError;
    Serial.printf("%s: HTTP %d\n", tag, code);
    http.end();
    return false;
  }
  int sz = http.getSize();
  Serial.printf("%s: HTTP 200 content-length=%d\n", tag, sz);
  bool ok = parseStream(http.getStream(), sz);
  http.end();
  if (!ok) {
    gLink = LinkState::ParseError;
    Serial.printf("%s: parse error\n", tag);
    return false;
  }
  return true;
}

static bool pollData() {
  if (WiFi.status() != WL_CONNECTED) {
    gLink = LinkState::Offline;
    snprintf(gLastErr, sizeof(gLastErr), "wifi down");
    return false;
  }

  if (!httpFetchParse("poll")) return false;

  if (!gSnap.valid || gSnap.pointCount <= 0) {
    Serial.println("poll: 0 points, triggering upstream refresh");
    triggerUpstreamRefresh();
    delay(TRIGGER_RETRY_MS);
    if (httpFetchParse("poll retry") && gSnap.valid && gSnap.pointCount > 0) {
      gLink = LinkState::Online;
      gLastErr[0] = 0;
      Serial.printf("poll ok (after trigger): pts=%d trend=%d C=%.0f H=%.0f A=%.0f G=%.0f O=%.0f\n",
                    gSnap.pointCount, gSnap.trendN,
                    gSnap.src[0].remainWeek, gSnap.src[1].remainWeek,
                    gSnap.src[2].remainWeek, gSnap.src[3].remainWeek,
                    gSnap.src[4].remainWeek);
      return true;
    }
    gLink = LinkState::Empty;
    snprintf(gLastErr, sizeof(gLastErr), "0 points");
    return false;
  }

  gLink = LinkState::Online;
  gLastErr[0] = 0;
  Serial.printf("poll ok: pts=%d trend=%d C=%.0f H=%.0f A=%.0f G=%.0f O=%.0f\n",
                gSnap.pointCount, gSnap.trendN,
                gSnap.src[0].remainWeek, gSnap.src[1].remainWeek,
                gSnap.src[2].remainWeek, gSnap.src[3].remainWeek,
                gSnap.src[4].remainWeek);
  return true;
}

// ---- drawing primitives ----
static void drawStatusContent(const char* title, const char* line2, const char* line3) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_helvB12_tf);
  strCenter(W / 2, H / 2 - 20, title);
  u8g2.setFont(u8g2_font_6x13_tf);
  if (line2) strCenter(W / 2, H / 2 + 6, line2);
  if (line3) strCenter(W / 2, H / 2 + 26, line3);
}

static void drawStatusScreen(const char* title, const char* line2, const char* line3) {
  drawStatusContent(title, line2, line3);
  u8g2.sendBuffer();
}

static void drawBottomBar(const char* extraHint) {
  u8g2.drawHLine(8, 258, W - 16);
  u8g2.setFont(u8g2_font_6x13_tf);
  const char* sl = linkLabel(gLink);
  u8g2.drawStr(10, 278, sl);
  int sx = 10 + u8g2.getStrWidth(sl) + 6;
  if (gLink == LinkState::Online) u8g2.drawDisc(sx + 3, 273, 3);
  else u8g2.drawCircle(sx + 3, 273, 3);

  char mid[40];
  if (extraHint && extraHint[0]) snprintf(mid, sizeof(mid), "%s", extraHint);
  else if (gLink == LinkState::Online && gSnap.ingestShort[0])
    snprintf(mid, sizeof(mid), "ingest %s", gSnap.ingestShort);
  else if (gLastErr[0]) snprintf(mid, sizeof(mid), "%s", gLastErr);
  else snprintf(mid, sizeof(mid), "auto 1m");

  // Same size as "live" for readability (was 5x8 — too small on RLCD)
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(sx + 14, 278, mid);

  char pg[8];
  snprintf(pg, sizeof(pg), "P%d", gPage);
  int batReserve = (gBat >= 0) ? 70 : 24;
  strRight(W - batReserve, 278, pg);

  drawBatteryRight(W - 12, 267, gBat);
}

// ---- pages ----
// line styles: 0 solid, 1 thick dotted, 2 thick dash-dot, 3 double solid,
// 4 thin dashed (single-pixel dashes)
static void plotSegment(int x0, int y0, int x1, int y1, int style) {
  if (style == 0) {
    u8g2.drawLine(x0, y0, x1, y1);
  } else if (style == 1) {
    // thick dotted: 2x2 blobs every few steps
    int dx = x1 - x0, dy = y1 - y0;
    int steps = max(abs(dx), abs(dy));
    if (steps <= 0) {
      u8g2.drawBox(x0, y0, 2, 2);
      return;
    }
    for (int i = 0; i <= steps; i += 4) {
      int x = x0 + (int)((long)dx * i / steps);
      int y = y0 + (int)((long)dy * i / steps);
      u8g2.drawBox(x, y - 1, 2, 3);  // thicker than 1px dots
    }
  } else if (style == 2) {
    // thick dash-dot: short solid runs, double thickness
    int dx = x1 - x0, dy = y1 - y0;
    int steps = max(abs(dx), abs(dy));
    if (steps <= 0) {
      u8g2.drawBox(x0, y0 - 1, 2, 3);
      return;
    }
    for (int i = 0; i <= steps; i++) {
      if ((i % 10) < 6) {
        int x = x0 + (int)((long)dx * i / steps);
        int y = y0 + (int)((long)dy * i / steps);
        u8g2.drawPixel(x, y);
        u8g2.drawPixel(x, y - 1);
        u8g2.drawPixel(x, y + 1);
      }
    }
  } else if (style == 4) {
    // thin dashed: single-pixel dashes with gaps
    int dx = x1 - x0, dy = y1 - y0;
    int steps = max(abs(dx), abs(dy));
    if (steps <= 0) {
      u8g2.drawPixel(x0, y0);
      return;
    }
    for (int i = 0; i <= steps; i++) {
      if ((i % 8) < 4) {
        int x = x0 + (int)((long)dx * i / steps);
        int y = y0 + (int)((long)dy * i / steps);
        u8g2.drawPixel(x, y);
      }
    }
  } else {
    // thick solid (double line)
    u8g2.drawLine(x0, y0, x1, y1);
    u8g2.drawLine(x0, y0 - 1, x1, y1 - 1);
  }
}

static void drawTrendSeries(int left, int top, int cw, int ch, int srcIdx, int style) {
  int n = gSnap.trendN;
  if (n < 2) return;

  // stream segment by segment; TREND_N-sized coordinate buffers would be too
  // much stack now that the window holds hundreds of points
  int px = 0, py = 0;
  bool pen = false;
  for (int i = 0; i < n; i++) {
    int8_t v = gSnap.trend[srcIdx][i];
    if (v < 0) {
      pen = false;
      continue;
    }
    int x = left + (int)((long)i * (cw - 1) / (n - 1));
    int y = top + ch - 1 - (int)((long)v * (ch - 1) / 100);
    if (pen) plotSegment(px, py, x, y, style);
    px = x;
    py = y;
    pen = true;
  }
}

static void renderTrend() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  gBat = readBatteryPct();

  u8g2.setFont(u8g2_font_helvB12_tf);
  u8g2.drawStr(10, 18, "WEEK REMAIN TREND");
  u8g2.setFont(u8g2_font_6x13_tf);
  char hdr[20];
  snprintf(hdr, sizeof(hdr), "LAST %d PTS", gSnap.trendN > 0 ? gSnap.trendN : TREND_N);
  strRight(W - 12, 16, hdr);
  u8g2.drawHLine(8, 24, W - 16);
  u8g2.drawHLine(8, 25, W - 16);

  // legend (5 sources: CLAUDE, CHIHYI, ALSTON, GROK, OLLAMA)
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(10, 40, "- CLAUDE");
  u8g2.drawStr(90, 40, ":: CHIHYI");
  u8g2.drawStr(170, 40, "-. ALSTON");
  u8g2.drawStr(260, 40, "= GROK");
  u8g2.drawStr(330, 40, "== OLLAMA");

  // chart taller after removing NOW strip
  const int left = 40, top = 50, cw = 346, ch = 188;
  u8g2.drawFrame(left, top, cw, ch);

  // y labels + daily guides (100/7)
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(4, top + 10, "100");
  u8g2.drawStr(10, top + ch / 2 + 4, "50");
  u8g2.drawStr(16, top + ch - 2, "0");
  for (int i = 1; i <= 6; i++) {
    int y = top + ch - 1 - (int)((long)(100.0f / 7.0f * i) * (ch - 1) / 100);
    for (int x = left + 2; x < left + cw - 2; x += 6)
      u8g2.drawPixel(x, y);
  }

  if (gSnap.trendN >= 2) {
    drawTrendSeries(left, top, cw, ch, 0, 0);  // CLAUDE solid
    drawTrendSeries(left, top, cw, ch, 1, 1);  // CHIHYI thick dotted
    drawTrendSeries(left, top, cw, ch, 2, 4);  // ALSTON thin dashed
    drawTrendSeries(left, top, cw, ch, 3, 2);  // GROK thick dash
    drawTrendSeries(left, top, cw, ch, 4, 3);  // OLLAMA double solid
  } else {
    u8g2.setFont(u8g2_font_6x13_tf);
    strCenter(left + cw / 2, top + ch / 2, "not enough points");
  }

  u8g2.setFont(u8g2_font_6x13_tf);
  if (gSnap.trendStartLbl[0]) u8g2.drawStr(left, top + ch + 14, gSnap.trendStartLbl);
  if (gSnap.trendEndLbl[0]) strRight(left + cw, top + ch + 14, gSnap.trendEndLbl);

  drawBottomBar(nullptr);  // show ingest / status (same size as live)
}

// ---- per-source 10d chart pages (P2-P5) ----
static int firstTrendValid(int srcIdx) {
  for (int i = 0; i < gSnap.trendN; i++) {
    if (gSnap.trend[srcIdx][i] >= 0 && gSnap.trendTs[i] > 100000) return i;
  }
  return -1;
}

static int lastTrendValid(int srcIdx) {
  for (int i = gSnap.trendN - 1; i >= 0; i--) {
    if (gSnap.trend[srcIdx][i] >= 0 && gSnap.trendTs[i] > 100000) return i;
  }
  return -1;
}

static long clampEpoch(long value, long lo, long hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

static int trendChartX(long ts, int left, int cw, long start, long end) {
  long span = end - start;
  if (span <= 0) return left;
  long long offset = (long long)(ts - start) * (cw - 1);
  return left + (int)(offset / span);
}

static int trendChartY(int remain, int top, int ch) {
  if (remain < 0) remain = 0;
  if (remain > 100) remain = 100;
  return top + ch - 1 - (int)((long)remain * (ch - 1) / 100L);
}

static void fmtChartMd(long epoch, char* out, size_t n) {
  if (epoch <= 100000) {
    snprintf(out, n, "--");
    return;
  }
  time_t t = (time_t)epoch;
  struct tm tm;
  localtime_r(&t, &tm);
  snprintf(out, n, "%02d/%02d", tm.tm_mon + 1, tm.tm_mday);
}

static int collectTrendResetEvents(int srcIdx) {
  gTrendEventN = 0;
  int previous = -1;
  for (int i = 0; i < gSnap.trendN; i++) {
    int currentRemain = gSnap.trend[srcIdx][i];
    if (currentRemain < 0 || gSnap.trendTs[i] <= 100000) continue;

    if (previous >= 0 && gTrendEventN < TREND_EVENT_MAX) {
      int previousRemain = gSnap.trend[srcIdx][previous];
      long previousTs = gSnap.trendTs[previous];
      long currentTs = gSnap.trendTs[i];
      long previousDue = gSnap.trendResetWeek[srcIdx][previous];
      long currentDue = gSnap.trendResetWeek[srcIdx][i];
      int jump = currentRemain - previousRemain;
      bool scheduleRolled = previousDue > 100000 && currentDue > 100000
                            && currentDue - previousDue > 45L * 60L;
      bool expectedInWindow = previousDue > 100000
                              && previousDue >= previousTs - 20L * 60L
                              && previousDue <= currentTs + 20L * 60L;

      if (jump >= 5 || (scheduleRolled && expectedInWindow)) {
        TrendResetEvent& event = gTrendEvents[gTrendEventN++];
        event.time = expectedInWindow ? clampEpoch(previousDue, previousTs, currentTs) : currentTs;
        event.due = currentDue;
        event.beforeRemain = previousRemain;
        event.afterRemain = currentRemain;
        event.confirmed = expectedInWindow || (scheduleRolled && jump >= 20);
      }
    }
    previous = i;
  }

  // Collapse duplicate markers caused by a simple/history-only pair around one reset.
  int write = 0;
  for (int i = 0; i < gTrendEventN; i++) {
    if (write > 0 && gTrendEvents[i].time - gTrendEvents[write - 1].time < 10L * 60L) {
      if (gTrendEvents[i].confirmed) gTrendEvents[write - 1] = gTrendEvents[i];
      continue;
    }
    if (write != i) gTrendEvents[write] = gTrendEvents[i];
    write++;
  }
  gTrendEventN = write;
  return gTrendEventN;
}

static float trendUsedSlope(int srcIdx, int firstIdx, int lastIdx, long cycleStart) {
  int count = 0;
  float sumX = 0.0f;
  float sumY = 0.0f;
  for (int i = firstIdx; i <= lastIdx; i++) {
    if (gSnap.trend[srcIdx][i] < 0 || gSnap.trendTs[i] < cycleStart) continue;
    float x = (float)(gSnap.trendTs[i] - cycleStart) / 86400.0f;
    float y = 100.0f - (float)gSnap.trend[srcIdx][i];
    sumX += x;
    sumY += y;
    count++;
  }
  if (count < 3) return -1.0f;

  float meanX = sumX / count;
  float meanY = sumY / count;
  float numerator = 0.0f;
  float denominator = 0.0f;
  for (int i = firstIdx; i <= lastIdx; i++) {
    if (gSnap.trend[srcIdx][i] < 0 || gSnap.trendTs[i] < cycleStart) continue;
    float x = (float)(gSnap.trendTs[i] - cycleStart) / 86400.0f;
    float y = 100.0f - (float)gSnap.trend[srcIdx][i];
    numerator += (x - meanX) * (y - meanY);
    denominator += (x - meanX) * (x - meanX);
  }
  return denominator > 0.0f ? numerator / denominator : -1.0f;
}

static void drawTrendIdeal(int srcIdx, int left, int top, int cw, int ch,
                           long domainStart, long domainEnd) {
  int first = firstTrendValid(srcIdx);
  if (first < 0) return;

  if (gTrendEventN <= 0) {
    long due = gSnap.src[srcIdx].resetWeek;
    if (due > gSnap.trendTs[first]) {
      long t0 = max(domainStart, gSnap.trendTs[first]);
      long t1 = min(domainEnd, due);
      long span = due - gSnap.trendTs[first];
      int startUsed = 100 - gSnap.trend[srcIdx][first];
      int remain0 = 100 - startUsed;
      int remain1 = span > 0 ? 100 - (int)((long long)100 * (t1 - gSnap.trendTs[first]) / span) : 0;
      if (t1 > t0) plotSegment(trendChartX(t0, left, cw, domainStart, domainEnd),
                               trendChartY(remain0, top, ch),
                               trendChartX(t1, left, cw, domainStart, domainEnd),
                               trendChartY(remain1, top, ch), 1);
    }
    return;
  }

  for (int i = 0; i < gTrendEventN; i++) {
    const TrendResetEvent& event = gTrendEvents[i];
    long due = event.due > event.time ? event.due : gSnap.src[srcIdx].resetWeek;
    long nextEvent = (i + 1 < gTrendEventN) ? gTrendEvents[i + 1].time : due;
    long segmentEnd = due > 100000 ? min(due, nextEvent) : nextEvent;
    if (segmentEnd <= event.time) continue;

    long t0 = max(domainStart, event.time);
    long t1 = min(domainEnd, segmentEnd);
    if (t1 <= t0) continue;

    int remain0 = due > event.time
                    ? 100 - (int)((long long)100 * (t0 - event.time) / (due - event.time))
                    : 100;
    int remain1 = due > event.time
                    ? 100 - (int)((long long)100 * (t1 - event.time) / (due - event.time))
                    : 0;
    if (remain0 < 0) remain0 = 0;
    if (remain0 > 100) remain0 = 100;
    if (remain1 < 0) remain1 = 0;
    if (remain1 > 100) remain1 = 100;
    plotSegment(trendChartX(t0, left, cw, domainStart, domainEnd),
                trendChartY(remain0, top, ch),
                trendChartX(t1, left, cw, domainStart, domainEnd),
                trendChartY(remain1, top, ch), 1);
  }
}

static void renderSourceTrend(int srcIdx) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  gBat = readBatteryPct();

  int first = firstTrendValid(srcIdx);
  int last = lastTrendValid(srcIdx);
  if (first < 0 || last < 0) {
    drawStatusContent(gSnap.src[srcIdx].name, "NO WEEK DATA", "poll again");
    return;
  }

  collectTrendResetEvents(srcIdx);
  long dataStart = gSnap.trendTs[first];
  long dataEnd = gSnap.trendTs[last];
  long nextDue = gSnap.src[srcIdx].resetWeek;
  long domainEnd = nextDue > dataEnd ? nextDue : dataEnd;
  if (domainEnd <= dataStart) domainEnd = dataStart + 86400L;

  int latestEvent = -1;
  for (int i = 0; i < gTrendEventN; i++) {
    if (gTrendEvents[i].time <= dataEnd) latestEvent = i;
  }

  long cycleStart = latestEvent >= 0 ? gTrendEvents[latestEvent].time : dataStart;
  long cycleDue = latestEvent >= 0 && gTrendEvents[latestEvent].due > cycleStart
                    ? gTrendEvents[latestEvent].due : nextDue;
  int cycleFirst = first;
  while (cycleFirst <= last && gSnap.trendTs[cycleFirst] < cycleStart) cycleFirst++;

  float target = -1.0f;
  if (cycleDue > cycleStart) {
    int startUsed = latestEvent >= 0 ? 0 : 100 - gSnap.trend[srcIdx][first];
    float days = (float)(cycleDue - cycleStart) / 86400.0f;
    if (days > 0.0f) target = (100.0f - startUsed) / days;
  }

  float actual = -1.0f;
  if (cycleFirst <= last && dataEnd - cycleStart >= MIN_SLOPE_WINDOW_SEC)
    actual = trendUsedSlope(srcIdx, cycleFirst, last, cycleStart);

  const char* pace = "--";
  if (actual >= 0.0f && target > 0.0f) {
    if (actual > target * (1.0f + SLOPE_TOLERANCE_SEVERE)) pace = "VERY FAST";
    else if (actual > target * (1.0f + SLOPE_TOLERANCE)) pace = "FAST";
    else if (actual < target * (1.0f - SLOPE_TOLERANCE_SEVERE)) pace = "VERY SLOW";
    else if (actual < target * (1.0f - SLOPE_TOLERANCE)) pace = "SLOW";
    else pace = "ON PACE";
  } else if (latestEvent >= 0 && dataEnd - cycleStart < MIN_SLOPE_WINDOW_SEC) {
    pace = "RESET";
  }

  char title[24], meta[32], slopeLine[40], actualStr[8], targetStr[8];
  snprintf(title, sizeof(title), "%s REMAIN", gSnap.src[srcIdx].name);
  snprintf(meta, sizeof(meta), "P%d  LAST 10D", CHART_PAGE_BASE + srcIdx);
  if (actual >= 0.0f) snprintf(actualStr, sizeof(actualStr), "%.1f", actual);
  else snprintf(actualStr, sizeof(actualStr), "--");
  if (target >= 0.0f) snprintf(targetStr, sizeof(targetStr), "%.1f", target);
  else snprintf(targetStr, sizeof(targetStr), "--");
  snprintf(slopeLine, sizeof(slopeLine), "R %.0f%%  S %s  T %s/d",
           gSnap.trend[srcIdx][last] * 1.0f, actualStr, targetStr);

  u8g2.setFont(u8g2_font_helvB12_tf);
  u8g2.drawStr(10, 18, title);
  u8g2.setFont(u8g2_font_6x13_tf);
  strRight(W - 12, 16, meta);
  u8g2.drawStr(10, 38, slopeLine);
  strRight(W - 12, 38, pace);
  u8g2.drawHLine(8, 44, W - 16);

  const int left = 40, top = 50, cw = 346, ch = 188;
  u8g2.drawFrame(left, top, cw, ch);
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(4, top + 10, "100");
  u8g2.drawStr(10, top + ch / 2 + 4, "50");
  u8g2.drawStr(16, top + ch - 2, "0");
  for (int i = 1; i <= 3; i++) {
    int y = top + ch - 1 - (int)((long)(25 * i) * (ch - 1) / 100L);
    for (int x = left + 2; x < left + cw - 2; x += 6) u8g2.drawPixel(x, y);
  }

  drawTrendIdeal(srcIdx, left, top, cw, ch, dataStart, domainEnd);

  for (int i = 0; i < gTrendEventN; i++) {
    const TrendResetEvent& event = gTrendEvents[i];
    if (event.time < dataStart || event.time > domainEnd) continue;
    int x = trendChartX(event.time, left, cw, dataStart, domainEnd);
    for (int y = top + 2; y < top + ch - 2; y += 6) u8g2.drawVLine(x, y, 3);
    u8g2.drawCircle(x, trendChartY(event.afterRemain, top, ch), 2);
    u8g2.drawStr(x + 3, top + 12, event.confirmed ? "R" : "?");
  }

  if (nextDue > dataEnd && nextDue <= domainEnd) {
    int x = trendChartX(nextDue, left, cw, dataStart, domainEnd);
    for (int y = top + 2; y < top + ch - 2; y += 6) u8g2.drawVLine(x, y, 3);
    u8g2.drawStr(x - 18, top + 26, "DUE");
  }

  int previous = -1;
  for (int i = first; i <= last; i++) {
    if (gSnap.trend[srcIdx][i] < 0 || gSnap.trendTs[i] <= 100000) continue;
    int x = trendChartX(gSnap.trendTs[i], left, cw, dataStart, domainEnd);
    int y = trendChartY(gSnap.trend[srcIdx][i], top, ch);
    if (previous >= 0) {
      int px = trendChartX(gSnap.trendTs[previous], left, cw, dataStart, domainEnd);
      int py = trendChartY(gSnap.trend[srcIdx][previous], top, ch);
      u8g2.drawLine(px, py, x, y);
    }
    u8g2.drawPixel(x, y);
    previous = i;
  }

  char startLbl[8], endLbl[8];
  fmtChartMd(dataStart, startLbl, sizeof(startLbl));
  fmtChartMd(domainEnd, endLbl, sizeof(endLbl));
  u8g2.drawStr(left, top + ch + 14, startLbl);
  strRight(left + cw, top + ch + 14, endLbl);
  drawBottomBar("R=reset D=due");
}

// P0 — 5 horizontal rows, one per source. Each row: name+origin (top line),
// big remain% + bar + 5h/day/week-reset (bottom line), pace right-aligned.
static void renderCombined() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  gBat = readBatteryPct();

  u8g2.setFont(u8g2_font_helvB12_tf);
  u8g2.drawStr(10, 18, "ALL DETAIL");
  u8g2.setFont(u8g2_font_6x13_tf);
  strRight(W - 12, 16, "REMAIN=100-USED");
  u8g2.drawHLine(8, 24, W - 16);
  u8g2.drawHLine(8, 25, W - 16);

  float day[N_SOURCES];
  float best = -1.0f;
  int bestIdx = -1;
  for (int i = 0; i < N_SOURCES; i++) {
    day[i] = dailyBudgetPct(gSnap.src[i]);
    if (day[i] > best) { best = day[i]; bestIdx = i; }
  }

  // 5 rows in the content area (y=26..255). Row height ~44px.
  const int rowTop[N_SOURCES] = {30, 75, 120, 165, 210};
  const int rowH = 44;

  // separator lines between rows
  for (int i = 0; i < N_SOURCES - 1; i++) {
    int y = rowTop[i] + rowH - 2;
    u8g2.drawHLine(8, y, W - 16);
  }

  for (int i = 0; i < N_SOURCES; i++) {
    const SourceUi& s = gSnap.src[i];
    int ty = rowTop[i];

    // --- top line: name + origin (left), pace (right) ---
    u8g2.setFont(u8g2_font_helvB10_tf);
    u8g2.drawStr(10, ty + 12, s.name);
    if (s.origin[0]) {
      int nw = u8g2.getStrWidth(s.name);
      u8g2.setFont(u8g2_font_6x13_tf);
      u8g2.drawStr(10 + nw + 4, ty + 12, s.origin);
    }
    u8g2.setFont(u8g2_font_helvB10_tf);
    strRight(W - 10, ty + 12, paceLabel(day[i]));

    // --- bottom line: big remain% + bar + 5h/day/week-reset ---
    if (!s.present || !s.ok || s.remainWeek < 0) {
      u8g2.setFont(u8g2_font_logisoso16_tn);
      u8g2.drawStr(10, ty + 36, "--");
      u8g2.setFont(u8g2_font_6x13_tf);
      u8g2.drawStr(10 + 32, ty + 34, s.present ? "source fail" : "missing");
      continue;
    }

    // big remain number (16pt) + % sign
    char num[12];
    snprintf(num, sizeof(num), "%.0f", s.remainWeek);
    u8g2.setFont(u8g2_font_logisoso16_tn);
    u8g2.drawStr(10, ty + 36, num);
    int nw = u8g2.getStrWidth(num);
    u8g2.setFont(u8g2_font_helvB10_tf);
    u8g2.drawStr(10 + nw + 1, ty + 34, "%");

    // progress bar after the number
    int barX = 10 + nw + 14;
    int barW = 70;
    drawBar(barX, ty + 27, barW, 9, s.remainWeek / 100.0f);

    // metrics line: 5H:xx%  DAY:xx%*  W reset xx
    char five[12], dayS[12], rw[16], line[56];
    if (s.remain5h < 0) snprintf(five, sizeof(five), "--");
    else snprintf(five, sizeof(five), "%.0f%%", s.remain5h);
    if (day[i] < 0.0f) snprintf(dayS, sizeof(dayS), "--");
    else snprintf(dayS, sizeof(dayS), "%.0f%%", day[i]);
    fmtReset(s.resetWeek, rw, sizeof(rw));

    snprintf(line, sizeof(line), "5H:%s  D:%s%s  W:%s", five, dayS,
             (i == bestIdx && day[i] >= 0.0f) ? "*" : "", rw);

    u8g2.setFont(u8g2_font_6x13_tf);
    int mtrX = barX + barW + 8;
    u8g2.drawStr(mtrX, ty + 34, line);
  }

  drawBottomBar(nullptr);  // show ingest time / status
}

static void render() {
  if ((gLink == LinkState::WifiSetup || gLink == LinkState::Connecting || gLink == LinkState::Booting)
      && !gSnap.valid) {
    drawStatusContent(
      gLink == LinkState::WifiSetup ? "WIFI SETUP" :
      gLink == LinkState::Connecting ? "CONNECTING" : "BOOTING",
      gLink == LinkState::WifiSetup ? "AP: AIUsage-RLCD" : "aiusage-web",
      gLink == LinkState::WifiSetup ? "open 192.168.4.1" : DATA_URL);
  } else {
    if (gPage == 0) renderCombined();
    else if (gPage == 1) renderTrend();
    else if (gPage < CHART_PAGE_BASE + N_SOURCES) renderSourceTrend(gPage - CHART_PAGE_BASE);
    else renderCombined();
  }

  drawUpdateBadge();
  u8g2.sendBuffer();
}

// Small "UPD" badge in top-right corner to signal fresh data was fetched.
// Drawn after page render so it overlays regardless of current page.
static void drawUpdateBadge() {
  if (!gShowUpdateBadge) return;
  // Inverted box: black background, white text
  u8g2.setDrawColor(1);
  u8g2.drawBox(W - 28, 0, 28, 12);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(W - 26, 10, "UPD");
  u8g2.setDrawColor(1);
}

// ---- WiFi (connect only for poll / portal; radio OFF between) ----
static bool secretsConfigured() {
  if (!WIFI_SSID[0]) return false;
  if (strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0) return false;
  return true;
}

static void radioOff() {
  if (WiFi.getMode() == WIFI_OFF) return;
  WiFi.disconnect(true);
  delay(30);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi radio off");
}

static bool ensureWifi(uint32_t timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);

  if (secretsConfigured()) {
    Serial.printf("WiFi begin SSID=%s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  } else {
    // NVS credentials from prior WiFiManager session
    Serial.println("WiFi begin (saved creds)");
    WiFi.begin();
  }

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(200);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi OK %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.println("WiFi connect fail");
  return false;
}

// Connect → GET /data → always radio off after (success or fail).
static void pollCycle() {
  if (!ensureWifi(WIFI_CONNECT_MS)) {
    if (!gSnap.valid) {
      gLink = LinkState::Offline;
      snprintf(gLastErr, sizeof(gLastErr), "wifi down");
    } else {
      // Keep showing last snapshot; Online→idle in linkLabel when radio off.
      snprintf(gLastErr, sizeof(gLastErr), "wifi fail");
    }
    radioOff();
    return;
  }

  pollData();
  radioOff();
}

static void startWifiPortal() {
  gLink = LinkState::WifiSetup;
  drawStatusScreen("WIFI SETUP", "AP: AIUsage-RLCD", "open 192.168.4.1");
  WiFi.mode(WIFI_STA);  // portal needs radio on
  WiFiManager wm;
  wm.setConfigPortalTimeout(300);
  bool ok = wm.startConfigPortal("AIUsage-RLCD");
  if (!ok) {
    if (WiFi.status() != WL_CONNECTED) {
      if (!gSnap.valid) gLink = LinkState::Offline;
      snprintf(gLastErr, sizeof(gLastErr), "portal timeout");
    } else {
      configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov", "ntp.aliyun.com");
      pollData();
    }
  } else {
    Serial.printf("WiFi OK %s\n", WiFi.localIP().toString().c_str());
    configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov", "ntp.aliyun.com");
    pollData();
  }
  radioOff();
  render();
  noteDrawnMinute();
}

static void connectWiFi() {
  gLink = LinkState::Connecting;
  render();

  if (secretsConfigured()) {
    if (ensureWifi(WIFI_CONNECT_MS)) return;
    Serial.println("WiFi secrets failed, opening portal");
  }

  // Prefer saved credentials via autoConnect (NVS from previous portal)
  gLink = LinkState::WifiSetup;
  render();
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConfigPortalTimeout(300);
  bool ok = wm.autoConnect("AIUsage-RLCD");
  if (!ok) {
    gLink = LinkState::Offline;
    snprintf(gLastErr, sizeof(gLastErr), "portal timeout");
    Serial.println("WiFi portal failed");
    radioOff();
    return;
  }
  Serial.printf("WiFi OK %s\n", WiFi.localIP().toString().c_str());
}

// ---- buttons ----
static void handleButton(uint32_t now) {
  int b = digitalRead(BTN_BOOT);

  if (gLastBtn == HIGH && b == LOW) {
    gBtnDownMs = now;
    gLongPressFired = false;
  }

  if (b == LOW && gBtnDownMs && !gLongPressFired && (now - gBtnDownMs >= LONG_PRESS_MS)) {
    gLongPressFired = true;
    Serial.println("BOOT long-press → WiFi portal");
    startWifiPortal();
    gBtnDownMs = 0;
    // portal returns later; restart auto-page so it doesn't immediately flip
    gLastPageChange = millis();
  }

  if (gLastBtn == LOW && b == HIGH) {
    uint32_t held = gBtnDownMs ? (now - gBtnDownMs) : 0;
    if (!gLongPressFired && held >= 30 && held < LONG_PRESS_MS) {
      // manual flip: page immediately; deferred poll after 10s of inactivity
      gLastManualFlip = now;
      advancePage(now, "BOOT");
    }
    gBtnDownMs = 0;
    gLongPressFired = false;
  }

  gLastBtn = b;
}

// ---- setup / loop ----
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("aiusage_home: boot (P0-P6 / 5m page / 15m poll / radio-off)");

  pinMode(BTN_BOOT, INPUT_PULLUP);
  gLastBtn = digitalRead(BTN_BOOT);
  analogReadResolution(12);

  SPI.begin(RLCD_SCK, -1, RLCD_MOSI, RLCD_CS);
  u8g2.begin();
  u8g2.setBusClock(24000000);
  drawStatusScreen("AI USAGE", "BOOT = pages", "hold 3s = WiFi");

  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov", "ntp.aliyun.com");
    struct tm t;
    getLocalTime(&t, 5000);
    gLink = LinkState::Connecting;
    render();
    pollData();
    radioOff();  // radio only for poll; time keeps running locally
  } else {
    gLink = LinkState::Offline;
    radioOff();
  }

  render();
  noteDrawnMinute();
  gLastPoll = gLastRender = gLastPageChange = millis();
  Serial.println("aiusage_home: setup done");
}

void loop() {
  uint32_t now = millis();
  handleButton(now);

  // Auto page flip every AUTO_PAGE_MS; BOOT short-press resets the timer via advancePage.
  bool busySetup = (gLink == LinkState::WifiSetup || gLink == LinkState::Connecting
                    || gLink == LinkState::Booting);
  if (!busySetup && (now - gLastPageChange >= AUTO_PAGE_MS)) {
    advancePage(now, "auto");
  }

  // Deferred poll: if user manually flipped and then stayed on the same page
  // for 10 seconds, fetch cloud data and refresh the screen.
  static const uint32_t MANUAL_POLL_DELAY_MS = 10000;
  if (gLastManualFlip && !busySetup && (now - gLastManualFlip >= MANUAL_POLL_DELAY_MS)) {
    gLastManualFlip = 0;  // one-shot
    Serial.println("deferred poll: 10s idle after manual flip");
    pollCycle();  // ensureWifi → GET → radioOff
    gLastPoll = now;
    gShowUpdateBadge = true;
    gUpdateBadgeUntil = now + 3000;  // badge visible for 3 seconds
    render();
    noteDrawnMinute();
    gLastRender = now;
  } else if (now - gLastPoll >= POLL_MS) {
    pollCycle();  // ensureWifi → GET → radioOff
    render();
    noteDrawnMinute();
    gLastPoll = gLastRender = now;
  } else if (minuteChanged()) {
    // Clock/battery: full redraw only when local minute ticks
    render();
    gLastRender = now;
  } else if (gLastDrawnMinuteKey < 0 && (now - gLastRender >= RENDER_MS)) {
    // No NTP yet: occasional redraw so UI is not frozen forever
    render();
    gLastRender = now;
  }

  // Clear update badge after timeout
  if (gShowUpdateBadge && (int32_t)(now - gUpdateBadgeUntil) >= 0) {
    gShowUpdateBadge = false;
    render();
    gLastRender = now;
  }

  delay(50);
}
