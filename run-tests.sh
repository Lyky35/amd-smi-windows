#!/usr/bin/env bash
# Builds and runs the host-side unit tests. These cover the
# platform-independent logic only (argument parsing, formatting, renderers);
# the ADLX calls themselves require a Windows host.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="${TMPDIR:-/tmp}/amd-smi-win-tests"

echo "==> Compiling host tests"
g++ -std=c++17 -Wall -Wextra -Werror \
    -DAMD_SMI_VERSION=\"test\" \
    -I"$ROOT/src" \
    "$ROOT/tests/test_logic.cpp" \
    "$ROOT/src/cli.cpp" \
    "$ROOT/src/output.cpp" \
    "$ROOT/src/box.cpp" \
    "$ROOT/src/console.cpp" \
    "$ROOT/src/layout.cpp" \
    "$ROOT/src/default_view.cpp" \
    -o "$BIN"

echo "==> Running"
"$BIN"
