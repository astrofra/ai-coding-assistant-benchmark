#!/usr/bin/env bash
# Fetch the Flora (Calendar #3) GIF sprites from John Maeda's original site.
# Idempotent: re-running only re-downloads missing/corrupt files.
set -euo pipefail

BASE_URL="${FLORA_ASSET_URL:-http://maedastudio.com/1997/cal3/ims}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST="$HERE/../assets/ims"

mkdir -p "$DEST"

SPRITES=(
  shiseido maeda2 maeda1
  b0 b1 b2 b3 b4 b5 b6 b7 b8 b9
  r0 r1 r2 r3 r4 r5 r6 r7 r8 r9
)

for name in "${SPRITES[@]}"; do
  out="$DEST/$name.gif"
  if [ -s "$out" ]; then
    echo "ok      $name.gif"
    continue
  fi
  echo "fetch   $name.gif"
  curl -fsSL --max-time 30 -o "$out" "$BASE_URL/$name.gif"
done

echo "Sprites in $DEST"
