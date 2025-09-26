#!/bin/bash

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build the project
if command -v nproc >/dev/null 2>&1; then
    make -j$(nproc)
else
    # macOS doesn't have nproc, use sysctl instead
    make -j$(sysctl -n hw.ncpu)
fi