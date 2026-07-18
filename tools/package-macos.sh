#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build/macos"
APP="$OUT/CodexDeck.app"
CONTENTS="$APP/Contents"
RESOURCES="$CONTENTS/Resources"
BRIDGE="$RESOURCES/Bridge"

if [[ -z "${DEVELOPER_DIR:-}" && -d /Applications/Xcode-beta.app/Contents/Developer ]]; then
  export DEVELOPER_DIR=/Applications/Xcode-beta.app/Contents/Developer
fi

if [[ "$(uname -m)" != "arm64" ]]; then
  echo "CodexDeck 0.1.0 packaging currently targets Apple Silicon only." >&2
  exit 1
fi

cd "$ROOT"
pnpm build
"$ROOT/tools/mac-swift.sh" test
"$ROOT/tools/mac-swift.sh" release

rm -rf "$OUT"
mkdir -p "$CONTENTS/MacOS" "$RESOURCES/Runtime" "$RESOURCES/Docs"
cp "$ROOT/apps/macos/Resources/Info.plist" "$CONTENTS/Info.plist"
cp "$ROOT/apps/macos/Resources/AppIcon.icns" "$RESOURCES/AppIcon.icns"

BIN_PATH="$(cd "$ROOT/apps/macos" && swift build -c release --show-bin-path)"
cp "$BIN_PATH/CodexDeckCompanion" "$CONTENTS/MacOS/CodexDeckCompanion"

pnpm --config.inject-workspace-packages=true --filter @codexdeck/bridge deploy --prod "$BRIDGE"
rm -rf "$BRIDGE/src" "$BRIDGE/test" "$BRIDGE/config/bridge.local.yaml" "$BRIDGE/config/workflows.local.yaml"
# Guard against workspace self-links that would point outside the bundle and
# invalidate strict macOS bundle signing.
rm -f "$BRIDGE/node_modules/.pnpm/node_modules/@codexdeck/bridge"

NODE_SOURCE="$(command -v node)"
NODE_REAL="$(node -e 'process.stdout.write(require("node:fs").realpathSync(process.argv[1]))' "$NODE_SOURCE")"
cp "$NODE_REAL" "$RESOURCES/Runtime/node"
chmod 755 "$RESOURCES/Runtime/node" "$CONTENTS/MacOS/CodexDeckCompanion"
cp "$ROOT/docs/hardware-test-checklist.md" "$RESOURCES/Docs/hardware-test-checklist.md"
cp "$ROOT/docs/troubleshooting.md" "$RESOURCES/Docs/troubleshooting.md"

IDENTITY="${CODE_SIGN_IDENTITY:-}"
if [[ -z "$IDENTITY" ]]; then
  IDENTITY="$(security find-identity -v -p codesigning | awk -F\" '/Apple Development/{print $2; exit}')"
fi
if [[ -z "$IDENTITY" ]]; then
  IDENTITY="-"
fi

codesign --force --options runtime --timestamp=none --sign "$IDENTITY" \
  --entitlements "$ROOT/apps/macos/Resources/Node.entitlements" \
  "$RESOURCES/Runtime/node"
codesign --force --options runtime --timestamp=none --sign "$IDENTITY" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

ditto -c -k --keepParent "$APP" "$OUT/CodexDeck-0.1.0-arm64.zip"
echo "$APP"
echo "$OUT/CodexDeck-0.1.0-arm64.zip"
