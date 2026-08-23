#!/usr/bin/env bash
# Compile + upload ESP32-S3-RLCD-4.2 firmware with the locked board options.
# FQBN must stay in sync with docs/firmware-operations.md.
set -euo pipefail

FQBN='esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=huge_app,FlashSize=16M,PSRAM=opi'
LIBS="${HOME}/Documents/Arduino/libraries"
SKETCH="aiusage_home"
DO_COMPILE=1
DO_VERIFY=1
PORT=""

usage() {
  cat <<'EOF'
Usage: flash.sh [aiusage_home|hello_rlcd] [--upload-only] [--no-verify] [--port PATH]
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --upload-only)
      DO_COMPILE=0
      shift
      ;;
    --no-verify)
      DO_VERIFY=0
      shift
      ;;
    --port)
      if [[ $# -lt 2 || -z "${2:-}" ]]; then
        echo "flash.sh: --port requires a path" >&2
        usage >&2
        exit 1
      fi
      PORT="${2:-}"
      shift 2
      ;;
    aiusage_home|hello_rlcd)
      SKETCH="$1"
      shift
      ;;
    *)
      echo "flash.sh: unknown arg: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"
SKETCH_PATH="${ROOT}/firmware/${SKETCH}"

if [[ ! -d "$SKETCH_PATH" ]]; then
  echo "flash.sh: missing sketch ${SKETCH_PATH}" >&2
  exit 1
fi

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "flash.sh: arduino-cli not found" >&2
  exit 1
fi

discover_port() {
  python3 - "$ROOT" <<'PY'
import json, glob, subprocess, sys
out = subprocess.check_output(["arduino-cli", "board", "list", "--format", "json"], text=True)
data = json.loads(out or "{}")
ports = data.get("detected_ports") or data.get("ports") or []
cands = []
for item in ports:
    port = item.get("port") or {}
    addr = port.get("address") or ""
    if not addr:
        continue
    label = (port.get("label") or "") + " " + (item.get("matching_boards") or [{}])[0].get("name", "")
    blob = (addr + " " + label).lower()
    if any(s in blob for s in ("usbmodem", "usbserial", "wchusbserial", "slab_usb", "esp32", "jtag")):
        cands.append(addr)
if not cands:
    cands = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/cu.usbserial*") + glob.glob("/dev/cu.wchusbserial*") + glob.glob("/dev/cu.SLAB_USBtoUART"))
print(cands[0] if cands else "")
PY
}

wait_port() {
  local tries=12
  local found=""
  while (( tries-- > 0 )); do
    found="$(discover_port)"
    if [[ -n "$found" ]]; then
      echo "$found"
      return 0
    fi
    sleep 1
  done
  return 1
}

if [[ -z "$PORT" ]]; then
  PORT="$(discover_port)"
fi
if [[ -z "$PORT" ]]; then
  echo "flash.sh: no USB serial port. Plug the board in, then retry." >&2
  arduino-cli board list >&2 || true
  exit 2
fi

echo "flash.sh: sketch=${SKETCH} port=${PORT}"
echo "flash.sh: fqbn=${FQBN}"

if [[ "$DO_COMPILE" -eq 1 ]]; then
  compile_args=(compile --fqbn "$FQBN")
  if [[ -d "$LIBS" ]]; then
    compile_args+=(--libraries "$LIBS")
  fi
  compile_args+=("$SKETCH_PATH")
  arduino-cli "${compile_args[@]}"
fi

upload_once() {
  arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH_PATH"
}

if ! upload_once; then
  echo "flash.sh: upload failed, rescanning port and retrying once..." >&2
  sleep 2
  PORT="$(wait_port || true)"
  if [[ -z "$PORT" ]]; then
    echo "flash.sh: port disappeared. Hold BOOT, tap RST, release BOOT after 2s, then rerun with --upload-only." >&2
    exit 3
  fi
  echo "flash.sh: retry port=${PORT}"
  if ! upload_once; then
    echo "flash.sh: upload failed again. Hold BOOT, tap RST, release BOOT after 2s, then rerun with --upload-only." >&2
    exit 3
  fi
fi

echo "flash.sh: upload ok"

if [[ "$DO_VERIFY" -ne 1 ]]; then
  exit 0
fi

sleep 2
PORT="$(wait_port || true)"
if [[ -z "$PORT" ]]; then
  echo "flash.sh: uploaded, but serial port did not return" >&2
  exit 4
fi

VERIFY_SKETCH="$SKETCH" VERIFY_PORT="$PORT" python3 - <<'PY'
import os, select, time, sys

port = os.environ["VERIFY_PORT"]
sketch = os.environ["VERIFY_SKETCH"]
needles = {
    "aiusage_home": (b"aiusage_home: boot", b"poll ok:", b"setup done"),
    "hello_rlcd": (b"hello_rlcd: boot", b"display begin OK", b"frame sent"),
}.get(sketch, (b"boot",))

try:
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
except OSError as e:
    print(f"flash.sh: cannot open {port}: {e}", file=sys.stderr)
    sys.exit(4)

got = b""
t0 = time.time()
try:
    while time.time() - t0 < 25:
        r, _, _ = select.select([fd], [], [], 0.5)
        if fd not in r:
            continue
        try:
            chunk = os.read(fd, 4096)
        except BlockingIOError:
            continue
        if not chunk:
            continue
        got += chunk
        sys.stdout.write(chunk.decode("utf-8", "replace"))
        sys.stdout.flush()
        if any(n in got for n in needles):
            break
finally:
    os.close(fd)

if not any(n in got for n in needles):
    print("flash.sh: no expected serial banner (board may still be booting)", file=sys.stderr)
    sys.exit(4)

if sketch == "aiusage_home" and b"poll ok:" not in got:
    print("flash.sh: firmware booted; cloud poll not confirmed yet", file=sys.stderr)
PY
verify_rc=$?
if [[ "$verify_rc" -ne 0 ]]; then
  exit 4
fi
echo "flash.sh: serial verify ok"
