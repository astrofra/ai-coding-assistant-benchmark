#!/usr/bin/env bash
#
# Idempotent fetch of the Maeda "Flora" sprite GIFs into cpp/assets/ims.
# The original sprites are hosted by maedastudio.com.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT="$ROOT/assets/ims"
BASE="http://www.maedastudio.com/1997/cal3/ims"

mkdir -p "$OUT"

NAMES="shiseido maeda2 maeda1 b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 r0 r1 r2 r3 r4 r5 r6 r7 r8 r9"

for name in $NAMES; do
  target="$OUT/$name.gif"
  if [[ -s "$target" ]]; then
    echo "skip $name (present)"
    continue
  fi
  echo "fetch $name"
  curl -fsSL -o "$target.tmp" "$BASE/$name.gif"
  mv "$target.tmp" "$target"
done

echo "Assets fetched into $OUT"
