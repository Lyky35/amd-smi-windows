#!/usr/bin/env bash
# Cross-compiles amd-smi.exe for Windows x86-64.
#
# Toolchain resolution order:
#   1. $MINGW_PREFIX   (explicit)
#   2. ./llvm-mingw    (repo-local, populated by ./fetch-toolchain.sh)
#   3. system mingw-w64 (x86_64-w64-mingw32-g++)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"

resolve_toolchain() {
  if [[ -n "${MINGW_PREFIX:-}" ]]; then
    echo "$MINGW_PREFIX"
    return
  fi
  if [[ -x "$ROOT/llvm-mingw/bin/x86_64-w64-mingw32-clang++" ]]; then
    echo "$ROOT/llvm-mingw"
    return
  fi
  if command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
    # Empty prefix: fall back to whatever is on PATH.
    echo ""
    return
  fi
  echo "error: no MinGW-w64 toolchain found." >&2
  echo "       Run ./fetch-toolchain.sh, set MINGW_PREFIX, or install mingw-w64." >&2
  exit 1
}

PREFIX="$(resolve_toolchain)"

CMAKE_ARGS=(
  -S "$ROOT"
  -B "$BUILD"
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchain-mingw.cmake"
)
if [[ -n "$PREFIX" ]]; then
  CMAKE_ARGS+=(-DMINGW_PREFIX="$PREFIX")
fi

echo "==> Configuring (toolchain prefix: ${PREFIX:-<system>})"
cmake "${CMAKE_ARGS[@]}"

echo "==> Building"
cmake --build "$BUILD" -j"$(nproc)"

echo
echo "==> Done: $BUILD/bin/amd-smi.exe"
file "$BUILD/bin/amd-smi.exe"
