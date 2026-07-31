#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 /path/to/reference.clap /path/to/candidate.clap" >&2
  exit 2
fi

REFERENCE=$(realpath "$1")
CANDIDATE=$(realpath "$2")
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR="$ROOT/tests/.build"
OUT_DIR="$BUILD_DIR/out"
mkdir -p "$BUILD_DIR" "$OUT_DIR"

CXX=${CXX:-g++}
COMMON=(-std=c++20 -O2 -I"$ROOT/external/clap/include")
"$CXX" "${COMMON[@]}" "$ROOT/tests/render_clap.cpp" -ldl -o "$BUILD_DIR/render_clap"
"$CXX" "${COMMON[@]}" "$ROOT/tests/state_roundtrip.cpp" -ldl -o "$BUILD_DIR/state_roundtrip"
"$CXX" "${COMMON[@]}" "$ROOT/tests/inspect_clap.cpp" -ldl -o "$BUILD_DIR/inspect_clap"

for scenario in 0 1 2 3 4 5 6 7; do
  "$BUILD_DIR/render_clap" "$REFERENCE" "$scenario" "$OUT_DIR/reference_${scenario}.raw"
  "$BUILD_DIR/render_clap" "$CANDIDATE" "$scenario" "$OUT_DIR/candidate_${scenario}.raw"
  cmp "$OUT_DIR/reference_${scenario}.raw" "$OUT_DIR/candidate_${scenario}.raw"
  echo "scenario $scenario: bit-identical"
done

"$BUILD_DIR/state_roundtrip" "$REFERENCE" "$OUT_DIR/reference.state"
"$BUILD_DIR/state_roundtrip" "$CANDIDATE" "$OUT_DIR/candidate.state"
cmp "$OUT_DIR/reference.state" "$OUT_DIR/candidate.state"
echo "state save: bit-identical"

"$BUILD_DIR/inspect_clap" "$REFERENCE" > "$OUT_DIR/reference.inspect"
"$BUILD_DIR/inspect_clap" "$CANDIDATE" > "$OUT_DIR/candidate.inspect"
cmp "$OUT_DIR/reference.inspect" "$OUT_DIR/candidate.inspect"
echo "parameter metadata: identical"
