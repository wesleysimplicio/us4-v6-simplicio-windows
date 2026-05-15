#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"

echo "US4 V6 Windows Edition local start helper"

if [[ ! -f CMakeLists.txt ]]; then
  echo "Runtime scaffold not present in this checkout."
  echo "Use 'npm run test:cli' and 'npm run pack:dry' for starter-layer validation."
  exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "CMake was not found in PATH."
  echo "Install CMake or open a shell that exposes the Visual Studio toolchain."
  exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
  echo "Ninja was not found in PATH."
  echo "Install Ninja or expose it in the current shell."
  exit 1
fi

if ! command -v cl >/dev/null 2>&1 && ! command -v clang-cl >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
  echo "No C++ compiler was found in PATH."
  echo "Install MSVC or Clang before using this helper."
  exit 1
fi

cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DUS4_ENABLE_CUDA=ON \
  -DUS4_ENABLE_DIRECTML=ON \
  -DUS4_ENABLE_VULKAN=ON

cmake --build "$BUILD_DIR" --target us4-cli

echo "Build completed. Suggested next commands:"
echo "./$BUILD_DIR/us4-cli.exe --probe"
echo "./$BUILD_DIR/us4-cli.exe run --model qwen-0.5b --prompt \"hi\""
