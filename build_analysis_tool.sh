#!/usr/bin/env bash
# Build the analysis_tool quickly without rebuilding the emulator.
set -euo pipefail

error_exit() {
    echo "Error on line $1" >&2
    exit 1
}
trap 'error_exit $LINENO' ERR

# Navigate to the existing build directory. Assume it has already been configured.
cd "$(dirname "$0")/analysis_tool"
mkdir -p build
cd build

# Compile the analysis tool
cmake --build .

# Run tests
ctest
