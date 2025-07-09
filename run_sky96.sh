#!/usr/bin/env bash
# Script to install dependencies, build Sky96 and the analysis tool,
# then run the emulator with the test ROM.
# Exits immediately on any error and prints the failing line number.
set -euo pipefail

error_exit() {
    echo "Error on line $1" >&2
    exit 1
}
trap 'error_exit $LINENO' ERR

# Step 1: Install build dependencies
echo "Installing build dependencies..."
sudo apt update
sudo apt install -y build-essential libsdl2-dev libpng-dev libfreetype6-dev libz-dev

# Step 2: Build and install Sky96 and all plugins
cd "$(dirname "$0")/sky96"
echo "Building Sky96 and plugins..."
./m64p_build.sh

echo "Installing Sky96..."
./m64p_install.sh

# Step 3: Build the analysis tool
cd ../analysis_tool
mkdir -p build
cd build
echo "Configuring analysis tool..."
cmake ..
echo "Compiling analysis tool..."
make

# Step 4: Run analysis tool tests
ctest

# Step 5: Launch Sky96 with analysis tool and test ROM
cd ../../sky96/test
echo "Launching Sky96 with analysis tool..."
./sky96 ../../my_original_rom1.z64
