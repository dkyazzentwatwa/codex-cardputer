#!/usr/bin/env bash
set -euo pipefail

PORT="${1:?Usage: $0 /dev/cu.usbmodemXXXX}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
arduino-cli compile --clean --profile waveshare firmware/waveshare_amoled_18
arduino-cli upload --profile waveshare --port "$PORT" firmware/waveshare_amoled_18
