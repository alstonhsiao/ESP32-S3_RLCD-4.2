/*
 * aiusage_home — AI week-remain dashboard on Waveshare ESP32-S3-RLCD-4.2
 *
 * Data: GET https://aiusage-web.zeabur.app/data
 * remain% = 100 - used_weekly_pct  (same as aiusage-web / Telegram)
 *
 * Pages (auto every 60s; short-press BOOT advances + resets timer):
 *   P0 Home   — clock + 2x2 week remain
 *   P1 Detail — week / 5h / reset table
 *   P2 Trend  — last N remain% polylines (4 series, thick styles)
 *   P3 Pace   — 24h budget table (remain/days, week end, 5h end)
 * Long-press BOOT 3s → WiFi setup portal (AIUsage-RLCD)
 *
 * Board: ESP32S3 Dev Module | CDC On Boot | Huge APP | Flash 16MB | OPI PSRAM
 * Display: ST7305 SPI SCK=11 MOSI=12 DC=5 CS=40 RST=41  (U8G2_R1 = 400x300)
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <time.h>
#include <WiFiManager.h>
#include "secrets.h"

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
#define PAGE_COUNT 4
#define TREND_N 40

static const char* DATA_URL = "https://aiusage-web.zeabur.app/data";
static const uint32_t POLL_MS = 300000;  // fetch cloud data every 5 minutes
static const uint32_t RENDER_MS = 30000;     // clock / battery refresh while parked on a page
static const uint32_t AUTO_PAGE_MS = 60000;  // auto flip every 1 minute
static const uint32_t LONG_PRESS_MS = 3000;

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
  SourceUi src[4] = {
    {"CLAUDE", "", false, false, -1, -1, 0, 0},
    {"CODEX",  "", false, false, -1, -1, 0, 0},
    {"GROK",   "", false, false, -1, -1, 0, 0},
    {"OLLAMA", "", false, false, -1, -1, 0, 0},
  };
  // trend: remain% 0..100, or -1 missing; chronological oldest→newest
  int8_t trend[4][TREND_N];
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

static void render();  // forward

static void advancePage(uint32_t now, const char* reason) {
  gPage = (gPage + 1) % PAGE_COUNT;
  gLastPageChange = now;
  gLastRender = now;
  Serial.printf("page → P%d (%s)\n", gPage, reason);
  render();
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

static void fmtResetLine(long resetUnix, char* out, size_t n) {
  char body[20];
  fmtReset(resetUnix, body, sizeof(body));
  if (strcmp(body, "--") == 0) snprintf(out, n, "RESET --");
  else if (strcmp(body, "now") == 0) snprintf(out, n, "RESET now");
  else snprintf(out, n, "RESET %s", body);
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

// Local HH:MM from epoch (device TZ via configTime).
static void fmtClockHm(long resetUnix, char* out, size_t n) {
  if (resetUnix <= 0) {
    snprintf(out, n, "--");
    return;
  }
  time_t t = (time_t)resetUnix;
  struct tm tm;
  localtime_r(&t, &tm);
  snprintf(out, n, "%02d:%02d", tm.tm_hour, tm.tm_min);
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
static const char* paceLabel(float daily) {
  if (daily < 0.0f) return "--";
  if (daily > 18.0f) return "SLOW";
  if (daily < 12.0f) return "FAST";
  return "OK";
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

static void labelFromPoint(JsonObject pt, char* out, size_t n) {
  // Prefer ts "2026-08-04T19:45:45+08:00" -> "08/04"
  const char* ts = pt["ts"] | "";
  if (ts[0] && strlen(ts) >= 10) {
    // YYYY-MM-DD...
    snprintf(out, n, "%.2s/%.2s", ts + 5, ts + 8);
    return;
  }
  long ep = pt["ts_epoch"] | 0L;
  if (ep > 100000) {
    time_t t = (time_t)ep;
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(out, n, "%02d/%02d", tm.tm_mon + 1, tm.tm_mday);
    return;
  }
  snprintf(out, n, "--");
}

static float remainFromUsed(JsonVariant used) {
  if (used.isNull()) return -1;
  float rem = 100.0f - used.as<float>();
  if (rem < 0) rem = 0;
  if (rem > 100) rem = 100;
  return rem;
}

static const char* linkLabel(LinkState s) {
  switch (s) {
    case LinkState::Booting: return "boot";
    case LinkState::WifiSetup: return "setup";
    case LinkState::Connecting: return "wifi...";
    case LinkState::Online: return "live";
    case LinkState::Offline: return "offline";
    case LinkState::HttpError: return "http err";
    case LinkState::ParseError: return "parse";
    case LinkState::Empty: return "no data";
    default: return "?";
  }
}

static void fillSourceFromJson(SourceUi& dst, JsonObject s) {
  dst.present = true;
  dst.ok = s["ok"] | false;
  shortOrigin(s["origin"] | "", dst.origin, sizeof(dst.origin));
  dst.remainWeek = remainFromUsed(s["used_weekly_pct"]);
  dst.remain5h = remainFromUsed(s["used_5h_pct"]);
  dst.resetWeek = s["resets_weekly_at"].isNull() ? 0 : s["resets_weekly_at"].as<long>();
  dst.reset5h = s["resets_5h_at"].isNull() ? 0 : s["resets_5h_at"].as<long>();
}

// ---- fetch ----
static bool parsePayload(const String& payload) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    snprintf(gLastErr, sizeof(gLastErr), "json %s", err.c_str());
    return false;
  }

  JsonArray points = doc["points"].as<JsonArray>();
  int nPts = points.size();
  gSnap.pointCount = nPts;
  shortIngest(doc["last_ingest_at"] | doc["exported_at"] | "", gSnap.ingestShort, sizeof(gSnap.ingestShort));

  for (int i = 0; i < 4; i++) {
    gSnap.src[i].ok = false;
    gSnap.src[i].present = false;
    gSnap.src[i].remainWeek = -1;
    gSnap.src[i].remain5h = -1;
    gSnap.src[i].resetWeek = 0;
    gSnap.src[i].reset5h = 0;
    gSnap.src[i].origin[0] = 0;
    for (int j = 0; j < TREND_N; j++) gSnap.trend[i][j] = -1;
  }
  gSnap.trendN = 0;
  gSnap.trendStartLbl[0] = 0;
  gSnap.trendEndLbl[0] = 0;

  if (nPts <= 0) {
    gSnap.valid = false;
    return true;
  }

  const char* keys[4] = {"claude", "codex", "grok", "ollama"};

  // last point → current UI
  JsonObject last = points[nPts - 1].as<JsonObject>();
  JsonObject sources = last["sources"].as<JsonObject>();
  if (sources.isNull()) {
    gSnap.valid = false;
    return true;
  }
  for (int i = 0; i < 4; i++) {
    JsonObject s = sources[keys[i]].as<JsonObject>();
    if (s.isNull()) continue;
    fillSourceFromJson(gSnap.src[i], s);
  }

  // trend window: last TREND_N points
  int start = nPts > TREND_N ? nPts - TREND_N : 0;
  gSnap.trendN = nPts - start;
  for (int ti = 0; ti < gSnap.trendN; ti++) {
    JsonObject pt = points[start + ti].as<JsonObject>();
    if (ti == 0) labelFromPoint(pt, gSnap.trendStartLbl, sizeof(gSnap.trendStartLbl));
    if (ti == gSnap.trendN - 1) labelFromPoint(pt, gSnap.trendEndLbl, sizeof(gSnap.trendEndLbl));
    JsonObject srcs = pt["sources"].as<JsonObject>();
    for (int i = 0; i < 4; i++) {
      if (srcs.isNull()) { gSnap.trend[i][ti] = -1; continue; }
      JsonObject s = srcs[keys[i]].as<JsonObject>();
      if (s.isNull() || !(s["ok"] | false) || s["used_weekly_pct"].isNull()) {
        gSnap.trend[i][ti] = -1;
        continue;
      }
      float rem = remainFromUsed(s["used_weekly_pct"]);
      gSnap.trend[i][ti] = (int8_t)(rem + 0.5f);
    }
  }

  gSnap.valid = true;
  return true;
}

static bool pollData() {
  if (WiFi.status() != WL_CONNECTED) {
    gLink = LinkState::Offline;
    snprintf(gLastErr, sizeof(gLastErr), "wifi down");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(12);

  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(12000);
  if (!http.begin(client, DATA_URL)) {
    gLink = LinkState::HttpError;
    snprintf(gLastErr, sizeof(gLastErr), "begin fail");
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    snprintf(gLastErr, sizeof(gLastErr), "HTTP %d", code);
    gLink = LinkState::HttpError;
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  if (!parsePayload(body)) {
    gLink = LinkState::ParseError;
    return false;
  }
  if (!gSnap.valid || gSnap.pointCount <= 0) {
    gLink = LinkState::Empty;
    snprintf(gLastErr, sizeof(gLastErr), "0 points");
    return false;
  }

  gLink = LinkState::Online;
  gLastErr[0] = 0;
  Serial.printf("poll ok: pts=%d trend=%d C=%.0f X=%.0f G=%.0f O=%.0f\n",
                gSnap.pointCount, gSnap.trendN,
                gSnap.src[0].remainWeek, gSnap.src[1].remainWeek,
                gSnap.src[2].remainWeek, gSnap.src[3].remainWeek);
  return true;
}

// ---- drawing primitives ----
static void drawStatusScreen(const char* title, const char* line2, const char* line3) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_helvB12_tf);
  strCenter(W / 2, H / 2 - 20, title);
  u8g2.setFont(u8g2_font_6x13_tf);
  if (line2) strCenter(W / 2, H / 2 + 6, line2);
  if (line3) strCenter(W / 2, H / 2 + 26, line3);
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

static void drawSourceCell(int x, int y, const SourceUi& s, bool withReset) {
  u8g2.setFont(u8g2_font_helvB10_tf);
  u8g2.drawStr(x, y, s.name);

  u8g2.setFont(u8g2_font_6x13_tf);
  if (s.origin[0]) u8g2.drawStr(x, y + 14, s.origin);

  if (!s.present || !s.ok || s.remainWeek < 0) {
    u8g2.setFont(u8g2_font_logisoso22_tn);
    u8g2.drawStr(x, y + 42, "--");
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(x, y + 58, s.present ? "source fail" : "missing");
    return;
  }

  char num[12];
  snprintf(num, sizeof(num), "%.0f", s.remainWeek);
  u8g2.setFont(u8g2_font_logisoso22_tn);
  u8g2.drawStr(x, y + 42, num);
  int nw = u8g2.getStrWidth(num);
  u8g2.setFont(u8g2_font_helvB10_tf);
  u8g2.drawStr(x + nw + 2, y + 40, "%");

  drawBar(x, y + 50, 172, 12, s.remainWeek / 100.0f);

  if (withReset) {
    char rb[24];
    fmtResetLine(s.resetWeek, rb, sizeof(rb));
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(x, y + 74, rb);
  }
}

// ---- pages ----
static void renderHome() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  gBat = readBatteryPct();

  struct tm t;
  bool haveT = haveLocalTime(&t);
  char hhmm[8] = "--:--";
  char dateLine[20] = "";
  if (haveT) {
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", t.tm_hour, t.tm_min);
    const char* wd[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    const char* mo[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    snprintf(dateLine, sizeof(dateLine), "%s · %s %d", wd[t.tm_wday], mo[t.tm_mon], t.tm_mday);
  }

  u8g2.setFont(u8g2_font_logisoso32_tn);
  u8g2.drawStr(8, 40, hhmm);
  u8g2.setFont(u8g2_font_6x13_tf);
  if (dateLine[0]) u8g2.drawStr(10, 56, dateLine);

  u8g2.setFont(u8g2_font_6x13_tf);
  strRight(W - 12, 16, "AI USAGE");
  strRight(W - 12, 32, "WEEK REMAIN");
  char meta[28];
  if (gSnap.pointCount > 0) snprintf(meta, sizeof(meta), "%d pts · cloud", gSnap.pointCount);
  else snprintf(meta, sizeof(meta), "cloud");
  strRight(W - 12, 48, meta);

  u8g2.drawHLine(8, 60, W - 16);
  u8g2.drawHLine(8, 61, W - 16);

  drawSourceCell(12, 74, gSnap.src[0], true);
  u8g2.drawVLine(200, 68, 96);
  drawSourceCell(212, 74, gSnap.src[1], true);

  u8g2.drawHLine(8, 168, W - 16);

  drawSourceCell(12, 180, gSnap.src[2], false);
  u8g2.drawVLine(200, 174, 76);
  drawSourceCell(212, 180, gSnap.src[3], false);

  drawBottomBar(nullptr);
  u8g2.sendBuffer();
}

static void renderDetail() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  gBat = readBatteryPct();

  u8g2.setFont(u8g2_font_helvB12_tf);
  u8g2.drawStr(10, 18, "USAGE DETAIL");
  u8g2.setFont(u8g2_font_6x13_tf);
  strRight(W - 12, 16, "REMAIN=100-USED");
  u8g2.drawHLine(8, 24, W - 16);
  u8g2.drawHLine(8, 25, W - 16);

  // column headers — same size as row values
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(10, 40, "SRC");
  u8g2.drawStr(90, 40, "WEEK");
  u8g2.drawStr(200, 40, "5H");
  u8g2.drawStr(290, 40, "RESET W");

  for (int i = 0; i < 4; i++) {
    const SourceUi& s = gSnap.src[i];
    int y0 = 48 + i * 50;
    bool warn = s.ok && ((s.remainWeek >= 0 && s.remainWeek < 10.0f) ||
                         (s.remain5h >= 0 && s.remain5h < 10.0f));

    if (warn) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(8, y0 - 2, 384, 46);
      u8g2.setDrawColor(0);  // inverted text on filled box
    } else {
      u8g2.setDrawColor(1);
    }

    u8g2.setFont(u8g2_font_helvB10_tf);
    u8g2.drawStr(12, y0 + 12, s.name);

    char week[12], five[12], rst[16];
    if (!s.present || !s.ok || s.remainWeek < 0) snprintf(week, sizeof(week), "--");
    else snprintf(week, sizeof(week), "%.0f%%", s.remainWeek);
    if (!s.present || !s.ok || s.remain5h < 0) snprintf(five, sizeof(five), "--");
    else snprintf(five, sizeof(five), "%.0f%%", s.remain5h);
    fmtReset(s.resetWeek, rst, sizeof(rst));

    u8g2.setFont(u8g2_font_helvB12_tf);
    u8g2.drawStr(90, y0 + 14, week);
    u8g2.drawStr(200, y0 + 14, five);
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(290, y0 + 14, rst);

    // mini week bar
    if (s.ok && s.remainWeek >= 0) {
      // when inverted, bar still uses current draw color (0 = "white" on black box)
      int bx = 90, by = y0 + 22, bw = 100, bh = 8;
      u8g2.drawFrame(bx, by, bw, bh);
      int fill = (int)((bw - 4) * (s.remainWeek / 100.0f) + 0.5f);
      if (fill > 0) u8g2.drawBox(bx + 2, by + 2, fill, bh - 4);
    }

    if (warn) {
      u8g2.setFont(u8g2_font_6x13_tf);
      char note[28];
      if (s.remain5h >= 0 && s.remain5h < 10.0f) {
        char r5[12];
        fmtReset(s.reset5h, r5, sizeof(r5));
        snprintf(note, sizeof(note), "5h LOW  reset %s", r5);
      } else {
        snprintf(note, sizeof(note), "WEEK LOW");
      }
      u8g2.drawStr(12, y0 + 40, note);
      u8g2.setDrawColor(1);
    } else if (i < 3) {
      u8g2.drawHLine(8, y0 + 44, W - 16);
    }
  }

  drawBottomBar(nullptr);
  u8g2.sendBuffer();
}

// line styles: 0 solid, 1 thick dotted, 2 thick dash-dot, 3 double solid
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
  } else {
    // thick solid (double line)
    u8g2.drawLine(x0, y0, x1, y1);
    u8g2.drawLine(x0, y0 - 1, x1, y1 - 1);
  }
}

static void drawTrendSeries(int left, int top, int cw, int ch, int srcIdx, int style) {
  int n = gSnap.trendN;
  if (n < 2) return;

  int xs[TREND_N], ys[TREND_N];
  int m = 0;
  for (int i = 0; i < n; i++) {
    int8_t v = gSnap.trend[srcIdx][i];
    if (v < 0) {
      for (int k = 0; k + 1 < m; k++)
        plotSegment(xs[k], ys[k], xs[k + 1], ys[k + 1], style);
      m = 0;
      continue;
    }
    int x = left + (n == 1 ? 0 : (int)((long)i * (cw - 1) / (n - 1)));
    int y = top + ch - 1 - (int)((long)v * (ch - 1) / 100);
    xs[m] = x;
    ys[m] = y;
    m++;
  }
  for (int k = 0; k + 1 < m; k++)
    plotSegment(xs[k], ys[k], xs[k + 1], ys[k + 1], style);
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

  // legend
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(10, 40, "- CLAUDE");
  u8g2.drawStr(110, 40, ":: CODEX");
  u8g2.drawStr(215, 40, "= GROK");
  u8g2.drawStr(295, 40, "== OLLAMA");

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
    drawTrendSeries(left, top, cw, ch, 1, 1);  // CODEX thick dotted
    drawTrendSeries(left, top, cw, ch, 2, 2);  // GROK thick dash
    drawTrendSeries(left, top, cw, ch, 3, 3);  // OLLAMA double solid
  } else {
    u8g2.setFont(u8g2_font_6x13_tf);
    strCenter(left + cw / 2, top + ch / 2, "not enough points");
  }

  u8g2.setFont(u8g2_font_6x13_tf);
  if (gSnap.trendStartLbl[0]) u8g2.drawStr(left, top + ch + 14, gSnap.trendStartLbl);
  if (gSnap.trendEndLbl[0]) strRight(left + cw, top + ch + 14, gSnap.trendEndLbl);

  drawBottomBar(nullptr);  // show ingest / status (same size as live)
  u8g2.sendBuffer();
}

// P3 — table: daily budget = week_remain% / days_until_weekly_reset
static void renderPace() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  gBat = readBatteryPct();

  u8g2.setFont(u8g2_font_helvB12_tf);
  u8g2.drawStr(10, 18, "24H BUDGET");
  u8g2.setFont(u8g2_font_6x13_tf);
  strRight(W - 12, 16, "remain/days");
  u8g2.drawHLine(8, 24, W - 16);
  u8g2.drawHLine(8, 25, W - 16);

  // column headers
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(10, 40, "SRC");
  u8g2.drawStr(88, 40, "DAY");
  u8g2.drawStr(160, 40, "WEEK");
  u8g2.drawStr(230, 40, "W END");
  u8g2.drawStr(320, 40, "5H END");

  // precompute daily budgets; invert row with highest day% (opportunity)
  float day[4];
  float best = -1.0f;
  int bestIdx = -1;
  for (int i = 0; i < 4; i++) {
    day[i] = dailyBudgetPct(gSnap.src[i]);
    if (day[i] > best) {
      best = day[i];
      bestIdx = i;
    }
  }

  for (int i = 0; i < 4; i++) {
    const SourceUi& s = gSnap.src[i];
    int y0 = 48 + i * 50;
    bool hi = (i == bestIdx && day[i] >= 0.0f);

    if (hi) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(8, y0 - 2, 384, 46);
      u8g2.setDrawColor(0);
    } else {
      u8g2.setDrawColor(1);
    }

    u8g2.setFont(u8g2_font_helvB10_tf);
    u8g2.drawStr(12, y0 + 12, s.name);
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(12, y0 + 28, paceLabel(day[i]));

    char dayS[12], weekS[12], wDays[12], wClk[8], hCd[12], hClk[8];

    if (day[i] < 0.0f) snprintf(dayS, sizeof(dayS), "--");
    else snprintf(dayS, sizeof(dayS), "%.0f%%", day[i]);

    if (!s.present || !s.ok || s.remainWeek < 0.0f) snprintf(weekS, sizeof(weekS), "--");
    else snprintf(weekS, sizeof(weekS), "%.0f%%", s.remainWeek);

    float dLeft = daysUntil(s.resetWeek);
    if (dLeft < 0.0f) {
      snprintf(wDays, sizeof(wDays), "--");
      snprintf(wClk, sizeof(wClk), "--");
    } else {
      if (dLeft < 1.0f) snprintf(wDays, sizeof(wDays), "%.0fh", dLeft * 24.0f);
      else snprintf(wDays, sizeof(wDays), "%.1fd", dLeft);
      fmtClockHm(s.resetWeek, wClk, sizeof(wClk));
    }

    if (!s.present || !s.ok || s.reset5h <= 0 || s.remain5h < 0.0f) {
      snprintf(hCd, sizeof(hCd), "--");
      snprintf(hClk, sizeof(hClk), "--");
    } else {
      fmtReset(s.reset5h, hCd, sizeof(hCd));
      fmtClockHm(s.reset5h, hClk, sizeof(hClk));
    }

    u8g2.setFont(u8g2_font_helvB12_tf);
    u8g2.drawStr(88, y0 + 18, dayS);
    u8g2.setFont(u8g2_font_helvB10_tf);
    u8g2.drawStr(160, y0 + 18, weekS);

    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(230, y0 + 12, wDays);
    u8g2.drawStr(230, y0 + 28, wClk);
    u8g2.drawStr(320, y0 + 12, hCd);
    u8g2.drawStr(320, y0 + 28, hClk);

    if (hi) {
      u8g2.setDrawColor(1);
    } else if (i < 3) {
      u8g2.drawHLine(8, y0 + 44, W - 16);
    }
  }

  drawBottomBar("DAY=Wrem/days");
  u8g2.sendBuffer();
}

static void render() {
  if ((gLink == LinkState::WifiSetup || gLink == LinkState::Connecting || gLink == LinkState::Booting)
      && !gSnap.valid) {
    drawStatusScreen(
      gLink == LinkState::WifiSetup ? "WIFI SETUP" :
      gLink == LinkState::Connecting ? "CONNECTING" : "BOOTING",
      gLink == LinkState::WifiSetup ? "AP: AIUsage-RLCD" : "aiusage-web",
      gLink == LinkState::WifiSetup ? "open 192.168.4.1" : DATA_URL);
    return;
  }

  if (gPage == 0) renderHome();
  else if (gPage == 1) renderDetail();
  else if (gPage == 2) renderTrend();
  else renderPace();
}

// ---- WiFi ----
static bool secretsConfigured() {
  if (!WIFI_SSID[0]) return false;
  if (strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0) return false;
  return true;
}

static void startWifiPortal() {
  gLink = LinkState::WifiSetup;
  drawStatusScreen("WIFI SETUP", "AP: AIUsage-RLCD", "open 192.168.4.1");
  WiFiManager wm;
  wm.setConfigPortalTimeout(300);
  bool ok = wm.startConfigPortal("AIUsage-RLCD");
  if (!ok) {
    // try keep existing STA if any
    if (WiFi.status() != WL_CONNECTED) {
      gLink = LinkState::Offline;
      snprintf(gLastErr, sizeof(gLastErr), "portal timeout");
    } else {
      gLink = LinkState::Online;
    }
  } else {
    Serial.printf("WiFi OK %s\n", WiFi.localIP().toString().c_str());
    configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov", "ntp.aliyun.com");
    pollData();
  }
  render();
}

static void connectWiFi() {
  gLink = LinkState::Connecting;
  render();

  if (secretsConfigured()) {
    Serial.printf("WiFi begin SSID=%s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
      delay(250);
      Serial.print('.');
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("WiFi OK %s\n", WiFi.localIP().toString().c_str());
      return;
    }
    Serial.println("WiFi secrets failed, opening portal");
  }

  // Prefer saved credentials via autoConnect (NVS from previous portal)
  gLink = LinkState::WifiSetup;
  render();
  WiFiManager wm;
  wm.setConfigPortalTimeout(300);
  bool ok = wm.autoConnect("AIUsage-RLCD");
  if (!ok) {
    gLink = LinkState::Offline;
    snprintf(gLastErr, sizeof(gLastErr), "portal timeout");
    Serial.println("WiFi portal failed");
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
      // manual flip + restart 10s auto-page timer
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
  Serial.println("aiusage_home: boot (P0-P3 + auto 1m / BOOT nav)");

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
  } else {
    gLink = LinkState::Offline;
  }

  render();
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

  if (now - gLastPoll >= POLL_MS) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      gLink = LinkState::Offline;
      snprintf(gLastErr, sizeof(gLastErr), "wifi down");
    } else {
      pollData();
    }
    render();
    gLastPoll = gLastRender = now;
  } else if (now - gLastRender >= RENDER_MS) {
    // refresh clock / battery without changing page
    render();
    gLastRender = now;
  }

  delay(15);
}
