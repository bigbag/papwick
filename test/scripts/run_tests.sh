#!/bin/bash
# Run all unit tests

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)/test/build/bin"

if [ ! -d "$BIN_DIR" ]; then
    echo "ERROR: $BIN_DIR not found. Run make test-build first."
    exit 1
fi

FAILED=0
RAN=0
for test_exe in "$BIN_DIR"/*; do
    [ -x "$test_exe" ] && [ -f "$test_exe" ] || continue
    RAN=$((RAN + 1))
    "$test_exe" || FAILED=$((FAILED + 1))
done

[ "$RAN" -gt 0 ] || { echo "ERROR: no test executables in $BIN_DIR"; exit 1; }
[ "$FAILED" -eq 0 ] || { echo "$FAILED test suite(s) failed"; exit 1; }
