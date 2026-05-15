param(
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

Write-Host "US4 V6 Windows Edition local start helper"

if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "Runtime scaffold not present in this checkout."
    Write-Host "Use 'npm run test:cli' and 'npm run pack:dry' for starter-layer validation."
    exit 1
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "CMake was not found in PATH."
    Write-Host "Open a Visual Studio Developer PowerShell or add CMake/Ninja to PATH before running this helper."
    exit 1
}

if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    Write-Host "Ninja was not found in PATH."
    Write-Host "Install the Visual Studio C++ tooling or expose Ninja in the current shell."
    exit 1
}

if (-not (Get-Command cl -ErrorAction SilentlyContinue) -and
    -not (Get-Command clang-cl -ErrorAction SilentlyContinue) -and
    -not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
    Write-Host "No C++ compiler was found in PATH."
    Write-Host "Install the 'Desktop development with C++' workload or open a Developer PowerShell that exposes MSVC/Clang."
    exit 1
}

cmake -S . -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DUS4_ENABLE_CUDA=ON `
  -DUS4_ENABLE_DIRECTML=ON `
  -DUS4_ENABLE_VULKAN=ON

cmake --build $BuildDir --target us4-cli

Write-Host "Build completed. Suggested next commands:"
Write-Host ".\\$BuildDir\\us4-cli.exe --probe"
Write-Host ".\\$BuildDir\\us4-cli.exe run --model qwen-0.5b --prompt `"hi`""
