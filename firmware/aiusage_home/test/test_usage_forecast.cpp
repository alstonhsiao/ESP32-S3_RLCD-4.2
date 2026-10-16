// Host checks for usage_forecast.h. Compile with the system C++ compiler.
#include "../usage_forecast.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <math.h>

using namespace usage_forecast;

static int gFail = 0;
static int gCheck = 0;

static void expect(bool ok, const char* name) {
  gCheck++;
  if (ok) return;
  gFail++;
  std::fprintf(stderr, "FAIL %s\n", name);
}

static void expectText(const char* got, const char* want, const char* name) {
  gCheck++;
  if (got && want && std::strcmp(got, want) == 0) return;
  gFail++;
  std::fprintf(stderr, "FAIL %s got=\"%s\" want=\"%s\"\n", name, got ? got : "(null)", want ? want : "(null)");
}

static ForecastInput baseIn() {
  ForecastInput in;
  in.remainPct = 50;
  in.resetEpoch = 1700000000L + 4L * 86400L;
  in.observedEpoch = 1700000000L;
  in.nowEpoch = 1700000000L;
  in.monthlyUsd = 20;
  in.present = true;
  in.ok = true;
  return in;
}

static void checkPaceBoundaries() {
  const double days = 3.5;  // ideal = 50 exactly
  expect(classifyPace(53, days).status == kPaceOn, "gap +3 ON PACE");
  expect(classifyPace(47, days).status == kPaceOn, "gap -3 ON PACE");
  expect(classifyPace(50, days).status == kPaceOn, "gap 0 ON PACE");
  expect(classifyPace(60, days).status == kPaceSlow, "gap +10 SLOW");
  expect(classifyPace(40, days).status == kPaceFast, "gap -10 FAST");
  expect(classifyPace(std::nextafter(53.0, 100.0), days).status == kPaceSlow, "just over +3 SLOW");
  expect(classifyPace(std::nextafter(47.0, 0.0), days).status == kPaceFast, "just under -3 FAST");
  expect(classifyPace(std::nextafter(60.0, 100.0), days).status == kPaceVerySlow, "just over +10 VERY SLOW");
  expect(classifyPace(std::nextafter(40.0, 0.0), days).status == kPaceVeryFast, "just under -10 VERY FAST");
  expect(classifyPace(std::nextafter(60.0, 0.0), days).status == kPaceSlow, "just under +10 still SLOW");
  expect(classifyPace(std::nextafter(40.0, 100.0), days).status == kPaceFast, "just over -10 still FAST");

  PaceResult hi = classifyPace(63.0, days);
  expect(hi.status == kPaceVerySlow && hi.hasGap && hi.gapPct == 13.0, "gap +13 VERY SLOW");
  PaceResult lo = classifyPace(36.0, days);
  expect(lo.status == kPaceVeryFast && lo.gapPct == -14.0, "gap -14 VERY FAST");

  PaceResult clamped = classifyPace(100.0, 14.0);
  expect(clamped.status == kPaceOn && clamped.idealPct == 100.0 && clamped.gapPct == 0.0,
         "days>7 ideal clamps to 100");
  expect(classifyPace(89.0, 14.0).status == kPaceVeryFast, "clamped ideal gap -11 VERY FAST");
  expect(classifyPace(96.0, 14.0).status == kPaceFast, "clamped ideal gap -4 FAST");
  expect(classifyPace(97.0, 14.0).status == kPaceOn, "clamped ideal gap -3 ON PACE");
  expect(classifyPace(90.0, 14.0).status == kPaceFast, "clamped ideal gap -10 FAST");

  expect(classifyPace(90.0, 0.0).status == kPaceReset, "reset due is RESET not VERY SLOW");
  expect(!classifyPace(90.0, 0.0).hasGap, "reset due has no gap");
  expect(classifyPace(50.0, -1.0).status == kPaceUnavailable, "negative days unavailable");
  expect(classifyPace(101.0, days).status == kPaceUnavailable, "remain 101 unavailable");
  expect(classifyPace(-1.0, days).status == kPaceUnavailable, "remain -1 unavailable");
  expect(classifyPace(NAN, days).status == kPaceUnavailable, "remain nan unavailable");
  expectText(paceText(kPaceOn), "ON PACE", "pace text on");
  expectText(paceText(kPaceVerySlow), "VERY SLOW", "pace text very slow");
  expectText(paceText(kPaceReset), "RESET", "pace text reset");
  expectText(paceText(kPaceUnavailable), "--", "pace text unavailable");
}

static void checkCost() {
  expect(forecastCostUsd(12.0, 20.0) == 0.60, "claude 12% → $0.60");
  expect(forecastCostUsd(100.0, 30.0) == 7.50, "grok 100% → $7.50");
  expect(forecastCostUsd(71.5, 21.0) == (21.0 / 4.0) * 72.0 / 100.0, "round half up 71.5 → 72");
  expect(jsRoundNonNeg(71.5) == 72, "js round 71.5");
  expect(jsRoundNonNeg(71.49) == 71, "js round 71.49");
  expect(forecastCostUsd(140.0, 20.0) == 5.0, "cost clamps 140 to 100");
  expect(forecastCostUsd(-4.0, 20.0) == 0.0, "cost clamps negative to 0");
  expect(forecastCostUsd(50.0, -1.0) < 0.0, "negative monthly is null");
  expect(monthlyPlanUsd(0) == 20 && monthlyPlanUsd(1) == 21 && monthlyPlanUsd(2) == 21
             && monthlyPlanUsd(3) == 30 && monthlyPlanUsd(4) == 20,
         "default plan prices");
}

static void checkForecast() {
  char text[40];
  const long now = 1700000000L;

  // Normal: d=3.5 e=3.5 R=56 → P=12 exactly.
  ForecastInput normal = baseIn();
  normal.remainPct = 56;
  normal.observedEpoch = now;
  normal.nowEpoch = now;
  normal.resetEpoch = now + (long)(3.5 * 86400.0);
  normal.monthlyUsd = 20;
  ForecastResult nr = forecastWeekly(normal);
  formatForecast(text, sizeof(text), nr);
  expect(nr.status == kFcLeft && nr.hasProjection, "normal projects");
  expect(nr.projectedRemainPct == 12.0, "normal P=12");
  expect(nr.displayRemainPct == 12, "normal display 12");
  expect(nr.costUsd == 0.60, "normal usd 0.60");
  expectText(text, "LEFT 12% ~$0.60", "normal text");

  ForecastInput exact = normal;
  exact.remainPct = 50;
  ForecastResult exactR = forecastWeekly(exact);
  expect(exactR.status == kFcLeft && exactR.projectedRemainPct == 0.0,
         "exact ideal pace stays zero despite floating point rounding");
  formatForecast(text, sizeof(text), exactR);
  expectText(text, "LEFT 0% ~$0.00", "exact pace is not early exhaustion");

  ForecastInput later = normal;
  later.nowEpoch += 3600;
  ForecastResult laterR = forecastWeekly(later);
  expect(laterR.projectedRemainPct == nr.projectedRemainPct && laterR.costUsd == nr.costUsd,
         "same snapshot forecast stays fixed as clock advances");

  // Early by 2.0 days. d=4 e=3 R=40 v=20 P=-40.
  ForecastInput early = baseIn();
  early.remainPct = 40;
  early.observedEpoch = now;
  early.nowEpoch = now;
  early.resetEpoch = now + 4L * 86400L;
  early.monthlyUsd = 20;
  ForecastResult er = forecastWeekly(early);
  formatForecast(text, sizeof(text), er);
  expect(er.status == kFcEarly && er.hasProjection && er.earlyDays == 2.0, "early 2d");
  expect(er.projectedRemainPct < 0.0, "early P negative");
  expectText(text, "EARLY 2.0D", "early text");

  // earlyDays = 0.05 → NEAR RST, no projected dollar line.
  ForecastInput near = baseIn();
  near.remainPct = 5900.0 / 139.0;  // solves early=0.05 at d=3 e=4
  near.observedEpoch = now;
  near.nowEpoch = now;
  near.resetEpoch = now + 3L * 86400L;
  ForecastResult nearR = forecastWeekly(near);
  formatForecast(text, sizeof(text), nearR);
  expect(nearR.earlyDays < 0.1 && nearR.earlyDays > 0.0, "near earlyDays");
  expect(nearR.status == kFcNearReset, "near status");
  expectText(text, "NEAR RST", "near text");

  // earlyDays = 0.1 stays EARLY (strict < 0.1).
  ForecastInput edge = near;
  edge.remainPct = 2900.0 / 69.0;  // solves early=0.1 at d=3 e=4
  ForecastResult edgeR = forecastWeekly(edge);
  formatForecast(text, sizeof(text), edgeR);
  expect(!(edgeR.earlyDays < 0.1) && edgeR.status == kFcEarly, "early 0.1 is EARLY");
  expectText(text, "EARLY 0.1D", "early 0.1 text");

  // Zero usage: v=0, P=100, no division. GROK plan → $7.50.
  ForecastInput zero = baseIn();
  zero.remainPct = 100;
  zero.observedEpoch = now;
  zero.nowEpoch = now;
  zero.resetEpoch = now + 2L * 86400L;
  zero.monthlyUsd = monthlyPlanUsd(3);
  ForecastResult zr = forecastWeekly(zero);
  formatForecast(text, sizeof(text), zr);
  expect(zr.status == kFcLeft && zr.dailyUsedPct == 0.0 && zr.projectedRemainPct == 100.0, "v=0 P=100");
  expectText(text, "LEFT 100% ~$7.50", "zero usage text");

  // Used up wins over warmup.
  ForecastInput used = baseIn();
  used.remainPct = 0;
  used.observedEpoch = now - 2L * 3600L;
  used.nowEpoch = now;
  used.resetEpoch = used.observedEpoch + 6L * 86400L + 19L * 3600L;  // e ≈ 0.2d
  ForecastResult ur = forecastWeekly(used);
  formatForecast(text, sizeof(text), ur);
  expect(ur.status == kFcUsedUp && !ur.hasProjection, "used up before warmup");
  expectText(text, "USED UP", "used up text");

  ForecastInput usedLater = used;
  usedLater.resetEpoch = usedLater.observedEpoch + 3L * 86400L;
  expect(forecastWeekly(usedLater).status == kFcUsedUp, "used up mid week");

  // Just reset: e=0 < 12h → WARMUP, and v is not evaluated.
  ForecastInput fresh = baseIn();
  fresh.remainPct = 100;
  fresh.observedEpoch = now;
  fresh.nowEpoch = now;
  fresh.resetEpoch = now + 7L * 86400L;
  ForecastResult fr = forecastWeekly(fresh);
  formatForecast(text, sizeof(text), fr);
  expect(fr.status == kFcWarmup && fr.elapsedDays == 0.0 && !fr.hasProjection, "just reset warmup");
  expectText(text, "WARMUP", "warmup text");

  ForecastInput warm = fresh;
  warm.remainPct = 90;
  warm.resetEpoch = now + 6L * 86400L + 14L * 3600L;  // e = 0.4d < 0.5
  expect(forecastWeekly(warm).status == kFcWarmup, "elapsed under 12h warmup");

  // Exactly 12h is enough history (e == 0.5 is not warmup).
  ForecastInput half = fresh;
  half.remainPct = 95;
  half.resetEpoch = now + 6L * 86400L + 12L * 3600L;  // e = 0.5, P = 30
  expect(forecastWeekly(half).status == kFcLeft, "elapsed 12h is not warmup");

  // Stale is strict > 6h. Exactly 6h still projects.
  ForecastInput aged = baseIn();
  aged.remainPct = 56;
  aged.nowEpoch = now;
  aged.observedEpoch = now - kStaleSec;
  aged.resetEpoch = aged.observedEpoch + (long)(3.5 * 86400.0);
  aged.monthlyUsd = 20;
  expect(forecastWeekly(aged).status == kFcLeft, "snapshot age == 6h still projects");
  aged.observedEpoch = now - kStaleSec - 1;
  ForecastResult stale = forecastWeekly(aged);
  formatForecast(text, sizeof(text), stale);
  expect(stale.status == kFcStale && !stale.hasProjection, "snapshot age > 6h stale");
  expectText(text, "STALE", "stale text");

  ForecastInput future = baseIn();
  future.nowEpoch = now;
  future.observedEpoch = now + 1;
  future.resetEpoch = now + 3L * 86400L;
  ForecastResult fut = forecastWeekly(future);
  formatForecast(text, sizeof(text), fut);
  expect(fut.status == kFcFuture && !fut.hasProjection, "future snapshot");
  expectText(text, "FUTURE", "future text");

  ForecastInput expired = baseIn();
  expired.nowEpoch = now;
  expired.observedEpoch = now - 3600;
  expired.resetEpoch = now;
  expired.remainPct = 40;
  ForecastResult exp = forecastWeekly(expired);
  formatForecast(text, sizeof(text), exp);
  expect(exp.status == kFcWaitReset && !exp.hasProjection, "reset == now waits");
  expectText(text, "WAIT RST", "wait text");
  expired.resetEpoch = now - 1;
  expect(forecastWeekly(expired).status == kFcWaitReset, "reset in the past waits");

  ForecastInput badWeek = baseIn();
  badWeek.nowEpoch = now;
  badWeek.observedEpoch = now;
  badWeek.resetEpoch = now + 8L * 86400L;
  badWeek.remainPct = 80;
  ForecastResult bw = forecastWeekly(badWeek);
  formatForecast(text, sizeof(text), bw);
  expect(bw.status == kFcBadWeek && !bw.hasProjection, "cycle longer than 7d");
  expectText(text, "BAD WEEK", "bad week text");

  ForecastInput noClock = baseIn();
  noClock.nowEpoch = kMinEpoch;
  ForecastResult nc = forecastWeekly(noClock);
  formatForecast(text, sizeof(text), nc);
  expect(nc.status == kFcNoClock && !nc.hasProjection, "clock at threshold invalid");
  expectText(text, "NO CLOCK", "no clock text");

  ForecastInput noSnap = baseIn();
  noSnap.nowEpoch = now;
  noSnap.observedEpoch = 0;
  noSnap.resetEpoch = now + 2L * 86400L;
  ForecastResult ns = forecastWeekly(noSnap);
  formatForecast(text, sizeof(text), ns);
  expect(ns.status == kFcNoSnap && !ns.hasProjection, "missing snapshot");
  expectText(text, "NO SNAP", "no snap text");
  noSnap.observedEpoch = kMinEpoch;
  expect(forecastWeekly(noSnap).status == kFcNoSnap, "snapshot epoch threshold");

  ForecastInput illegal = baseIn();
  illegal.remainPct = 140;
  expect(forecastWeekly(illegal).status == kFcNoData, "remain 140 illegal");
  illegal.remainPct = -5;
  expect(forecastWeekly(illegal).status == kFcNoData, "remain -5 illegal");
  illegal.remainPct = 40;
  illegal.resetEpoch = 0;
  ForecastResult nd = forecastWeekly(illegal);
  formatForecast(text, sizeof(text), nd);
  expect(nd.status == kFcNoData && !nd.hasProjection, "reset missing");
  expectText(text, "NO DATA", "no data text");
  illegal = baseIn();
  illegal.remainPct = NAN;
  expect(forecastWeekly(illegal).status == kFcNoData, "remain nan illegal");

  ForecastInput failed = baseIn();
  failed.ok = false;
  failed.remainPct = 56;
  ForecastResult sf = forecastWeekly(failed);
  formatForecast(text, sizeof(text), sf);
  expect(sf.status == kFcSourceFail && !sf.hasProjection, "source fail");
  expectText(text, "SRC FAIL", "source fail text");

  ForecastInput missing = baseIn();
  missing.present = false;
  missing.ok = false;
  ForecastResult ms = forecastWeekly(missing);
  formatForecast(text, sizeof(text), ms);
  expect(ms.status == kFcMissing && !ms.hasProjection, "source missing");
  expectText(text, "MISSING", "missing text");

  expect(daysUntilReset(now + 86400L, now) == 1.0, "days until 1");
  expect(daysUntilReset(now, now) == 0.0, "days until due");
  expect(daysUntilReset(now + 86400L, kMinEpoch) < 0.0, "days until bad clock");
}

int main() {
  checkPaceBoundaries();
  checkCost();
  checkForecast();
  if (gFail) {
    std::fprintf(stderr, "%d failed / %d checks\n", gFail, gCheck);
    return 1;
  }
  std::printf("ok %d checks\n", gCheck);
  return 0;
}
