#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP="$ROOT/build/macos/CodexDeck.app"
NODE="$APP/Contents/Resources/Runtime/node"
BRIDGE="$APP/Contents/Resources/Bridge/dist/index.js"
BRIDGE_MODULE="$APP/Contents/Resources/Bridge/dist/config.js"

test -x "$APP/Contents/MacOS/CodexDeckCompanion"
test -x "$NODE"
test -f "$BRIDGE"
test -f "$APP/Contents/Resources/Bridge/node_modules/@codexdeck/protocol/dist/index.js"
codesign --verify --deep --strict "$APP"
env -i PATH=/usr/bin:/bin "$NODE" --version
env -i PATH=/usr/bin:/bin "$NODE" -e "import(process.argv[1])" "$BRIDGE_MODULE"

TEMP_HOME="$(mktemp -d /tmp/codexdeck-app-home.XXXXXX)"
env -i HOME="$TEMP_HOME" PATH=/usr/bin:/bin TMPDIR=/tmp \
  "$APP/Contents/MacOS/CodexDeckCompanion" >/tmp/codexdeck-app-smoke.log 2>&1 &
PID=$!
sleep 3
if ! kill -0 "$PID" 2>/dev/null; then
  cat /tmp/codexdeck-app-smoke.log >&2
  exit 1
fi
kill "$PID"
wait "$PID" 2>/dev/null || true
rm -rf "$TEMP_HOME"
echo "CodexDeck packaged-app smoke test passed"
