#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PORT=${1:-}

if [ -z "$PORT" ]; then
  PORT=$(arduino-cli board list | awk '/\/dev\/cu\.usbmodem/ { print $1; exit }')
fi
if [ -z "$PORT" ]; then
  echo "No /dev/cu.usbmodem Cardputer port detected; test sketch is compile-ready only." >&2
  exit 1
fi
case "$PORT" in
  /dev/cu.usbmodem*) ;;
  *)
    echo "Refusing non-USB-modem port: $PORT" >&2
    exit 1
    ;;
esac

arduino-cli compile --profile adv "$ROOT/firmware/tests/control_deck_core"
echo "proof=compile-ready target=cardputer-adv sketch=control_deck_core"
stty -f "$PORT" 1200 || true
sleep 1
NEW_PORT=$(arduino-cli board list | awk '/\/dev\/cu\.usbmodem/ { print $1; exit }')
if [ -n "$NEW_PORT" ]; then PORT=$NEW_PORT; fi
arduino-cli upload --profile adv -p "$PORT" "$ROOT/firmware/tests/control_deck_core"
echo "proof=uploaded target=cardputer-adv sketch=control_deck_core port=$PORT"

LOG=$(mktemp -t codexdeck-firmware-test.XXXXXX)
MONITOR_PID=""
cleanup() {
  if [ -n "$MONITOR_PID" ]; then kill "$MONITOR_PID" 2>/dev/null || true; fi
  rm -f "$LOG"
}
trap cleanup EXIT INT TERM
arduino-cli monitor -p "$PORT" --config baudrate=115200 >"$LOG" 2>&1 &
MONITOR_PID=$!

COUNT=0
while [ "$COUNT" -lt 45 ]; do
  if grep -q 'TEST SUMMARY' "$LOG"; then break; fi
  if ! kill -0 "$MONITOR_PID" 2>/dev/null; then break; fi
  sleep 1
  COUNT=$((COUNT + 1))
done
kill "$MONITOR_PID" 2>/dev/null || true
wait "$MONITOR_PID" 2>/dev/null || true
cat "$LOG"
if ! grep -q 'TEST SUMMARY passed=[0-9][0-9]* failed=0' "$LOG"; then
  echo "Firmware assertion summary missing or reported failures." >&2
  exit 1
fi
echo "proof=device-tested target=cardputer-adv sketch=control_deck_core"
