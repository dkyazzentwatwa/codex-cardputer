#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CONFIG="$ROOT/apps/bridge/config/bridge.local.yaml"

if [ ! -f "$CONFIG" ]; then
  echo "Missing $CONFIG" >&2
  echo "Copy bridge.example.yaml to bridge.local.yaml and configure workflows.local.yaml first." >&2
  exit 1
fi

cd "$ROOT"
exec env CODEXDECK_CONFIG="$CONFIG" pnpm dev
