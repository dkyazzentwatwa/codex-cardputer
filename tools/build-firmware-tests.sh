#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
arduino-cli compile --profile adv "$ROOT/firmware/tests/control_deck_core"
echo "proof=compile-ready target=cardputer-adv sketch=control_deck_core"
