#!/usr/bin/env bash
# Downloads the ADLX SDK from AMD's official GitHub repository into
# ./vendor/ADLX so the source can be published without redistributing AMD's
# SDK materials (their license covers distribution of your object code only).
#
# The upstream layout lives under <repo>/SDK/..., which matches this project's
# expected vendor/ADLX/{Include,ADLXHelper,Platform} structure after the copy.
#
# Network access is required on the machine performing the build. The SDK is
# NOT committed to this repository.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_REPO="${ADLX_REPO:-GPUOpen-LibrariesAndSDKs/ADLX}"
SRC_REF="${ADLX_REPO_REF:-main}"
DEST="$ROOT/vendor/ADLX"

if [[ -e "$DEST/Include/ADLXVersion.h" ]]; then
  echo "==> ADLX SDK already present at $DEST (delete it to re-fetch)."
  exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "==> Cloning $SRC_REPO @ $SRC_REF (shallow)"
git clone --depth 1 --branch "$SRC_REF" "https://github.com/$SRC_REPO.git" "$TMP/adlx"

VERSION="$(sed -nE 's/#define ADLX_VER_(MAJOR|MINOR|RELEASE|BUILD_NUM)[[:space:]]+([0-9]+)/\1=\2 /p' \
  "$TMP/adlx/SDK/Include/ADLXVersion.h" | tr '\n' ' ')"
echo "==> SDK requested: ${VERSION}"

mkdir -p "$DEST"
cp -r "$TMP/adlx/SDK/." "$DEST"

echo "==> ADLX SDK installed at $DEST"
echo "    (license: see vendor/ADLX/ADLX SDK License Agreement.pdf, fetched with the SDK)"