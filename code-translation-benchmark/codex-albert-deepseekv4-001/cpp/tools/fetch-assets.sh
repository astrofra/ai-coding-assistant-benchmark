#!/usr/bin/env bash
# Fetch the 22 raster sprites used by the Flora Calendar port into cpp/assets/ims/.
# Idempotent: skips files that already exist. Requires curl.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST_DIR="${SCRIPT_DIR}/../assets/ims"
BASE_URL="${FLORA_ASSET_BASE_URL:-http://maedastudio.com/1997/cal3/ims}"

mkdir -p "${DEST_DIR}"

sprites=(
    b0 b1 b2 b3 b4 b5 b6 b7 b8 b9
    r0 r1 r2 r3 r4 r5 r6 r7 r8 r9
    shiseido maeda2
)

failed=0
for name in "${sprites[@]}"; do
    target="${DEST_DIR}/${name}.gif"
    if [[ -f "${target}" ]]; then
        echo "skip   ${name}.gif (already present)"
        continue
    fi
    echo "fetch  ${name}.gif"
    if ! curl -fsSL "${BASE_URL}/${name}.gif" -o "${target}"; then
        echo "error  ${name}.gif (download failed)" >&2
        failed=1
    fi
done

if [[ ${failed} -ne 0 ]]; then
    echo "One or more sprites could not be fetched from ${BASE_URL}." >&2
    exit 1
fi

echo "All sprites present in ${DEST_DIR}."
