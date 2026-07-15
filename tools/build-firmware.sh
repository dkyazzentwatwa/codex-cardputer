#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
arduino-cli compile --profile adv "$ROOT/firmware/cardputer"
echo "proof=compile-ready target=cardputer-adv"
