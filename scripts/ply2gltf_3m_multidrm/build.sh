#!/bin/bash

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring project..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo "Building..."
make -j$(nproc)

echo "Build complete! Executable is in build/bin/ply2gltf_3m_multidrm"
