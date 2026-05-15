param(
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "use-msvc-toolchain.ps1")

Write-Host "US4 V6 Windows Edition local start helper"

if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "Runtime scaffold not present in this checkout."
    Write-Host "Use 'npm run test:cli' and 'npm run pack:dry' for starter-layer validation."
    exit 1
}

if (-not (Enable-MsvcToolchain)) {
    Write-Host "Visual Studio C++ tooling could not be discovered automatically."
    Write-Host "Install the 'Desktop development with C++' workload or expose MSVC/CMake/Ninja in the current shell."
    exit 1
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "CMake was not found after toolchain discovery."
    exit 1
}

cmake -S . -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DUS4_ENABLE_CUDA=ON `
  -DUS4_ENABLE_DIRECTML=ON `
  -DUS4_ENABLE_VULKAN=ON

cmake --build $BuildDir --target us4-cli

Write-Host "Build completed. Suggested next commands:"
Write-Host ".\\$BuildDir\\runtime\\cli\\us4-cli.exe --probe"
Write-Host ".\\$BuildDir\\runtime\\cli\\us4-cli.exe run --model qwen-0.5b --prompt `"hi`""
