#pragma once
// Types for the /data byte-stream scanner. Kept in a header so Arduino's
// auto-generated .ino prototypes can see them.

#ifndef N_SOURCES
#error "include usage_scan.h after N_SOURCES / TREND_N"
#endif

struct ScanPoint {
  bool hasEpoch = false;
  long epoch = 0;
  char iso[40] = "";
  bool present[N_SOURCES] = {false};
  bool hasError[N_SOURCES] = {false};
  bool hasOk[N_SOURCES] = {false};
  bool okFalse[N_SOURCES] = {false};
  bool hasWeekly[N_SOURCES] = {false};
  bool has5h[N_SOURCES] = {false};
  float weekly[N_SOURCES] = {0};
  float used5h[N_SOURCES] = {0};
  long resetWeek[N_SOURCES] = {0};
  long reset5h[N_SOURCES] = {0};
  char origin[N_SOURCES][16] = {{0}};
};

struct TrendSlot {
  long epoch = 0;
  int8_t remain[N_SOURCES];
  long resetWeek[N_SOURCES];
  char md[6];
};

struct ScanState {
  uint8_t mode = 0;  // 0 scan 1 str 2 esc 3 uni 4 num 5 lit
  char buf[44];
  size_t bufLen = 0;
  char uniHex[5] = "";
  uint8_t uniN = 0;
  int depth = 0;
  uint8_t nest[32];  // 1 = object, 0 = array
  bool expectKey = false;
  char pendingKey[24] = "";
  char valueKey[24] = "";
  int pointsDepth = -1;
  int pointDepth = -1;
  int sourcesDepth = -1;
  int srcIdx = -1;
  bool inPoint = false;
  bool seenIngest = false;
  ScanPoint pt;
  ScanPoint last;
  bool slotHas = false;
  long slotNo = -1;
  ScanPoint slotPt;
  long pointCount = 0;
};

// Everything the scanner needs to accumulate a snapshot without touching the
// UI's gSnap: a private trend ring and a staging snapshot the background poll
// fills, plus the ingest label scratch. The loop thread publishes the staged
// snapshot under a mutex after the poll task finishes.
struct ScanCtx {
  ScanState st;
  TrendSlot ring[TREND_N];
  int ringHead = 0;
  int ringN = 0;
  UsageSnapshot snap;      // staged result, published to gSnap on success
  char ingestShort[20] = "";
  bool seenIngest = false;
};

void scanCtxReset(ScanCtx& ctx);
