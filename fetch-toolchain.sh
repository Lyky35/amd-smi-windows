#!/usr/bin/env bash
# One-time setup: downloads a self-contained Linux-hosted MinGW-w64
# cross-compiler into ./llvm-mingw. No root required.
#
# Arch users who prefer system packages can instead run:
#   sudo pacman -S mingw-w64-gcc cmake
# and ./build.sh will pick up x86_64-w64-mingw32-g++ from PATH.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST="$ROOT/llvm-mingw"

if [[ -x "$DEST/bin/x86_64-w64-mingw32-clang++" ]]; then
  echo "llvm-mingw already present at $DEST"
  exit 0
fi

if ! command -v curl >/dev/null 2>&1; then
  echo "error: curl is required" >&2
  exit 1
fi
if ! command -v bsdtar >/dev/null 2>&1; then
  echo "error: bsdtar is required (libarchive, used to unpack .tar.xz)" >&2
  exit 1
fi

echo "==> Resolving latest llvm-mingw release"
ASSET_URL="$(curl -fsSL https://api.github.com/repos/mstorsjo/llvm-mingw/releases/latest \
  | grep -o '"browser_download_url": "[^"]*ucrt-ubuntu-22.04-x86_64\.tar\.xz"' \
  | head -1 | sed 's/.*: "//;s/"$//')"

if [[ -z "$ASSET_URL" ]]; then
  echo "error: could not locate a linux x86_64 llvm-mingw asset" >&2
  exit 1
fi
echo "    $ASSET_URL"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "==> Downloading (~80 MB)"
curl -fL --progress-bar -o "$TMP/toolchain.tar.xz" "$ASSET_URL"

echo "==> Extracting"
bsdtar xf "$TMP/toolchain.tar.xz" -C "$TMP"
rm -rf "$DEST"
mv "$TMP"/llvm-mingw-* "$DEST"

echo "==> Installed: $DEST"
"$DEST/bin/x86_64-w64-mingw32-clang++" --version | head -1
