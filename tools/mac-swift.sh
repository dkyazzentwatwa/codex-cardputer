#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -z "${DEVELOPER_DIR:-}" && -d /Applications/Xcode-beta.app/Contents/Developer ]]; then
  export DEVELOPER_DIR=/Applications/Xcode-beta.app/Contents/Developer
fi

action="${1:-test}"
case "$action" in
  build)
    swift build --package-path "$ROOT/apps/macos"
    ;;
  release)
    swift build -c release --package-path "$ROOT/apps/macos"
    ;;
  test)
    swift test --package-path "$ROOT/apps/macos"
    ;;
  *)
    echo "Usage: $0 {build|release|test}" >&2
    exit 2
    ;;
esac
