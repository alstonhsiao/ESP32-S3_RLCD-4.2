#pragma once
// P0 pace + weekly remainder forecast shared by the firmware and host tests.
// USD is an estimate of unused plan value (monthly fee / 4), not a refund.
// Pure C++11. No Arduino types.

#include <math.h>
#include <stdio.h>
#include <stddef.h>

namespace usage_forecast {

static const double kWeekDays = 7.0;
static const double kPaceInnerPct = 3.0;   // |gap| <= 3 → ON PACE
static const double kPaceOuterPct = 10.0;  // |gap| > 10 → VERY; 3 < |gap| <= 10 → SLOW/FAST
static const double kSecPerDay = 86400.0;
static const long kStaleSec = 6L * 3600L;
static const double kWarmupDays = 0.5;     // elapsed < 12h
static const long kMinEpoch = 100000L;

// Plan prices, source order: CLAUDE, CHIHYI, ALSTON, GROK, OLLAMA.
// Edit here. Weekly estimate = monthly/4 * rounded forecast remain%.
inline double monthlyPlanUsd(int sourceIndex) {
  switch (sourceIndex) {
    case 0: return 20.0;
    case 1: return 21.0;
    case 2: return 21.0;
    case 3: return 30.0;
    case 4: return 20.0;
    default: return -1.0;
  }
}

inline bool isFiniteNum(double v) { return isfinite(v) != 0; }

inline double clampPct(double v) {
  if (v < 0.0) return 0.0;
  if (v > 100.0) return 100.0;
  return v;
}

// JS Math.round for the non-negative percents this UI displays.
inline int jsRoundNonNeg(double v) {
  if (!isFiniteNum(v) || v <= 0.0) return 0;
  return (int)floor(v + 0.5);
}

// null → negative sentinel. Matches calculateForecastCostUsd, including clamp.
inline double forecastCostUsd(double projectedRemainPct, double monthlyUsd) {
  if (!isFiniteNum(projectedRemainPct) || !isFiniteNum(monthlyUsd) || monthlyUsd < 0.0)
    return -1.0;
  double remaining = clampPct(projectedRemainPct);
  return (monthlyUsd / 4.0) * (double)jsRoundNonNeg(remaining) / 100.0;
}

enum PaceStatus {
  kPaceUnavailable = 0,
  kPaceReset,
  kPaceVerySlow,
  kPaceSlow,
  kPaceOn,
  kPaceFast,
  kPaceVeryFast
};

struct PaceResult {
  PaceStatus status;
  bool hasGap;
  double gapPct;
  double idealPct;
};

// daysLeft == 0 (reset already due) is RESET, same as usage-web calculatePace.
// Otherwise gap = remain% - clamp(daysLeft/7*100, 0, 100).
// gap > 10 VERY SLOW, gap > 3 SLOW, gap < -10 VERY FAST, gap < -3 FAST, else ON PACE.
inline PaceResult classifyPace(double remainPct, double daysLeft) {
  PaceResult r;
  r.status = kPaceUnavailable;
  r.hasGap = false;
  r.gapPct = 0.0;
  r.idealPct = 0.0;
  if (!isFiniteNum(remainPct) || remainPct < 0.0 || remainPct > 100.0) return r;
  if (!isFiniteNum(daysLeft) || daysLeft < 0.0) return r;
  if (daysLeft == 0.0) {
    r.status = kPaceReset;
    return r;
  }
  double ideal = daysLeft / kWeekDays * 100.0;
  if (ideal < 0.0) ideal = 0.0;
  if (ideal > 100.0) ideal = 100.0;
  double gap = remainPct - ideal;
  r.hasGap = true;
  r.gapPct = gap;
  r.idealPct = ideal;
  if (gap > kPaceOuterPct) r.status = kPaceVerySlow;
  else if (gap > kPaceInnerPct) r.status = kPaceSlow;
  else if (gap < -kPaceOuterPct) r.status = kPaceVeryFast;
  else if (gap < -kPaceInnerPct) r.status = kPaceFast;
  else r.status = kPaceOn;
  return r;
}

inline const char* paceText(PaceStatus status) {
  switch (status) {
    case kPaceVerySlow: return "VERY SLOW";
    case kPaceSlow: return "SLOW";
    case kPaceOn: return "ON PACE";
    case kPaceFast: return "FAST";
    case kPaceVeryFast: return "VERY FAST";
    case kPaceReset: return "RESET";
    default: return "--";
  }
}

// Positive days until reset, 0 if the reset is due, -1 if the clock or reset is unusable.
inline double daysUntilReset(long resetEpoch, long nowEpoch) {
  if (resetEpoch <= kMinEpoch || nowEpoch <= kMinEpoch) return -1.0;
  if (resetEpoch <= nowEpoch) return 0.0;
  return (double)((long long)resetEpoch - (long long)nowEpoch) / kSecPerDay;
}

enum ForecastStatus {
  kFcLeft = 0,
  kFcEarly,
  kFcNearReset,
  kFcUsedUp,
  kFcWarmup,
  kFcWaitReset,
  kFcStale,
  kFcFuture,
  kFcNoSnap,
  kFcNoClock,
  kFcBadWeek,
  kFcNoData,
  kFcSourceFail,
  kFcMissing
};

struct ForecastInput {
  double remainPct;
  long resetEpoch;
  long observedEpoch;
  long nowEpoch;
  double monthlyUsd;
  bool present;
  bool ok;
};

struct ForecastResult {
  ForecastStatus status;
  bool hasProjection;  // true only when a remainder or early-day figure is real
  double daysLeft;
  double elapsedDays;
  double dailyUsedPct;
  double projectedRemainPct;
  double earlyDays;
  int displayRemainPct;
  double costUsd;
};

inline ForecastResult blankForecast(ForecastStatus status) {
  ForecastResult r;
  r.status = status;
  r.hasProjection = false;
  r.daysLeft = 0.0;
  r.elapsedDays = 0.0;
  r.dailyUsedPct = 0.0;
  r.projectedRemainPct = 0.0;
  r.earlyDays = 0.0;
  r.displayRemainPct = 0;
  r.costUsd = -1.0;
  return r;
}

// Anchor d/e/v/P to the snapshot, not the device clock.
// d=(reset-observed)/86400, e=7-d, v=(100-R)/e, P=R-v*d.
// v==0 skips the division and leaves P=R (100 at reset).
// Degraded statuses never set hasProjection.
inline ForecastResult forecastWeekly(const ForecastInput& in) {
  if (!in.present) return blankForecast(kFcMissing);
  if (!in.ok) return blankForecast(kFcSourceFail);
  if (in.nowEpoch <= kMinEpoch) return blankForecast(kFcNoClock);
  if (!isFiniteNum(in.remainPct) || in.remainPct < 0.0 || in.remainPct > 100.0
      || in.resetEpoch <= kMinEpoch) {
    return blankForecast(kFcNoData);
  }
  if (in.observedEpoch <= kMinEpoch) return blankForecast(kFcNoSnap);
  if (in.resetEpoch <= in.nowEpoch) return blankForecast(kFcWaitReset);
  if (in.observedEpoch > in.nowEpoch) return blankForecast(kFcFuture);
  if ((long long)in.nowEpoch - (long long)in.observedEpoch > (long long)kStaleSec)
    return blankForecast(kFcStale);

  double d = (double)((long long)in.resetEpoch - (long long)in.observedEpoch) / kSecPerDay;
  double e = kWeekDays - d;
  ForecastResult r = blankForecast(kFcBadWeek);
  r.daysLeft = d;
  r.elapsedDays = e;
  if (e < 0.0 || e >= kWeekDays) return r;
  if (in.remainPct == 0.0) {
    r.status = kFcUsedUp;
    return r;
  }
  if (e < kWarmupDays) {
    r.status = kFcWarmup;
    return r;
  }

  double used = 100.0 - in.remainPct;
  double v = (used == 0.0) ? 0.0 : used / e;
  double p = in.remainPct - v * d;
  // Fused multiply/subtract can leave an exact zero a few ulps negative.
  // Do not turn an exactly-on-budget week into an early-exhaustion forecast.
  if (fabs(p) < 1e-9) p = 0.0;
  r.dailyUsedPct = v;
  r.projectedRemainPct = p;
  if (p < 0.0) {
    if (v == 0.0) {
      r.status = kFcNearReset;
      return r;
    }
    r.earlyDays = d - in.remainPct / v;
    r.hasProjection = true;
    r.status = (r.earlyDays < 0.1) ? kFcNearReset : kFcEarly;
    return r;
  }
  r.hasProjection = true;
  r.status = kFcLeft;
  r.displayRemainPct = jsRoundNonNeg(clampPct(p));
  r.costUsd = forecastCostUsd(p, in.monthlyUsd);
  return r;
}

inline void formatForecast(char* out, size_t n, const ForecastResult& r) {
  if (!out || n == 0) return;
  switch (r.status) {
    case kFcLeft:
      if (r.costUsd < 0.0) snprintf(out, n, "LEFT %d%%", r.displayRemainPct);
      else snprintf(out, n, "LEFT %d%% ~$%.2f", r.displayRemainPct, r.costUsd);
      break;
    case kFcEarly:
      snprintf(out, n, "EARLY %.1fD", r.earlyDays);
      break;
    case kFcNearReset: snprintf(out, n, "NEAR RST"); break;
    case kFcUsedUp: snprintf(out, n, "USED UP"); break;
    case kFcWarmup: snprintf(out, n, "WARMUP"); break;
    case kFcWaitReset: snprintf(out, n, "WAIT RST"); break;
    case kFcStale: snprintf(out, n, "STALE"); break;
    case kFcFuture: snprintf(out, n, "FUTURE"); break;
    case kFcNoSnap: snprintf(out, n, "NO SNAP"); break;
    case kFcNoClock: snprintf(out, n, "NO CLOCK"); break;
    case kFcBadWeek: snprintf(out, n, "BAD WEEK"); break;
    case kFcSourceFail: snprintf(out, n, "SRC FAIL"); break;
    case kFcMissing: snprintf(out, n, "MISSING"); break;
    default: snprintf(out, n, "NO DATA"); break;
  }
}

}  // namespace usage_forecast
