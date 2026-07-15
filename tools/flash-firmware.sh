#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PORT=${1:-}

if [ -z "$PORT" ]; then
  PORT=$(arduino-cli board list | awk '/\/dev\/cu\.usbmodem/ { print $1; exit }')
fi

if [ -z "$PORT" ]; then
  echo "No /dev/cu.usbmodem Cardputer port detected." >&2
  exit 1
fi

case "$PORT" in
  /dev/cu.usbmodem*) ;;
  *)
    echo "Refusing non-USB-modem port: $PORT" >&2
    exit 1
    ;;
esac

arduino-cli compile --profile adv "$ROOT/firmware/cardputer"
echo "proof=compile-ready target=cardputer-adv"
stty -f "$PORT" 1200 || true
sleep 1
NEW_PORT=$(arduino-cli board list | awk '/\/dev\/cu\.usbmodem/ { print $1; exit }')
if [ -n "$NEW_PORT" ]; then
  PORT=$NEW_PORT
fi
arduino-cli upload --profile adv -p "$PORT" "$ROOT/firmware/cardputer"
echo "proof=uploaded target=cardputer-adv port=$PORT"
