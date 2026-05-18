$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "use-msvc-toolchain.ps1")

function Invoke-AndAssert {
    param(
        [Parameter(Mandatory = $true)]
        [scriptblock]$Command,
        [Parameter(Mandatory = $true)]
        [string]$FailureMessage
    )

    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw $FailureMessage
    }
}

Write-Host "Running current repository validation"

Invoke-AndAssert { npm run test:cli } "npm run test:cli failed."
Invoke-AndAssert { npm run pack:dry } "npm run pack:dry failed."

$tokens = $null
$errors = $null
[System.Management.Automation.Language.Parser]::ParseFile((Join-Path (Get-Location) "bootstrap.ps1"), [ref]$tokens, [ref]$errors) | Out-Null
if ($errors.Count -gt 0) {
    $errors | ForEach-Object { Write-Host $_.Message }
    exit 1
}

foreach ($scriptPath in @(
    (Join-Path (Get-Location) "scripts\install-completions.ps1"),
    (Join-Path (Get-Location) "scripts\build-portable-zip.ps1"),
    (Join-Path (Get-Location) "scripts\build-msix.ps1"),
    (Join-Path (Get-Location) "scripts\create-dev-signing-cert.ps1"),
    (Join-Path (Get-Location) "scripts\dev-msix-smoke.ps1"),
    (Join-Path (Get-Location) "scripts\generate-checksums.ps1"),
    (Join-Path (Get-Location) "scripts\install-msix-smoke.ps1"),
    (Join-Path (Get-Location) "scripts\post-publish-smoke.ps1"),
    (Join-Path (Get-Location) "scripts\preflight-release.ps1"),
    (Join-Path (Get-Location) "scripts\render-project-status.ps1"),
    (Join-Path (Get-Location) "scripts\release-dry-run.ps1"),
    (Join-Path (Get-Location) "scripts\render-planning-status.ps1"),
    (Join-Path (Get-Location) "scripts\render-release-manifest.ps1"),
    (Join-Path (Get-Location) "scripts\render-release-notes.ps1"),
    (Join-Path (Get-Location) "scripts\remove-dev-signing-cert.ps1"),
    (Join-Path (Get-Location) "scripts\render-winget-manifests.ps1"),
    (Join-Path (Get-Location) "scripts\validate-publish-layout.ps1"),
    (Join-Path (Get-Location) "scripts\validate-release-assets.ps1"),
    (Join-Path (Get-Location) "scripts\validate-release-tag.ps1"),
    (Join-Path (Get-Location) "scripts\validate-winget-manifests.ps1"),
    (Join-Path (Get-Location) "scripts\sign-msix.ps1"),
    (Join-Path (Get-Location) "scripts\completions\us4-cli.ps1")
)) {
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile($scriptPath, [ref]$tokens, [ref]$errors) | Out-Null
    if ($errors.Count -gt 0) {
        $errors | ForEach-Object { Write-Host $_.Message }
        exit 1
    }
}

if ((Test-Path "CMakeLists.txt") -and (Test-Path "build")) {
    [void](Enable-MsvcToolchain)
    if ((Get-Command cmake -ErrorAction SilentlyContinue) -and
        (Get-Command ctest -ErrorAction SilentlyContinue)) {
        Write-Host "Detected runtime scaffold/build. Refreshing CMake configuration and rebuilding before CTest."
        Invoke-AndAssert { cmake -S . -B build } "cmake configuration failed."
        Invoke-AndAssert { cmake --build build -j 8 } "cmake build failed."
        Write-Host "Running CTest on the rebuilt runtime artifacts."
        Invoke-AndAssert { ctest --test-dir build --output-on-failure } "ctest failed."
    } else {
        Write-Host "Build directory found, but cmake/ctest is not available in PATH. Skipping runtime rebuild and test invocation."
    }
}

if (Test-Path (Join-Path "build" "runtime\\benchmarks\\correctness_diff.exe")) {
    Write-Host "Detected correctness gate binary. Running CPU scalar correctness validation."
    Invoke-AndAssert { & (Join-Path "build" "runtime\\benchmarks\\correctness_diff.exe") } "CPU scalar correctness gate failed."
}

if (Test-Path (Join-Path "build" "runtime\\benchmarks\\hybrid_planner_gate.exe")) {
    Write-Host "Detected hybrid planner gate binary. Running Vulkan/Windows ML correctness validation."
    Invoke-AndAssert { & (Join-Path "build" "runtime\\benchmarks\\hybrid_planner_gate.exe") } "Hybrid planner gate failed."
}

if (Test-Path (Join-Path "build" "runtime\\benchmarks\\cpu_block_gemm_bench.exe")) {
    Write-Host "Detected CPU block GEMM benchmark binary. Running oneDNN vs scalar local benchmark evidence."
    Invoke-AndAssert { & (Join-Path "build" "runtime\\benchmarks\\cpu_block_gemm_bench.exe") } "CPU block GEMM benchmark failed."
}

if (Test-Path (Join-Path "build" "runtime\\benchmarks\\cpu_attention_bench.exe")) {
    Write-Host "Detected CPU attention benchmark binary. Running AVX fused-attention local benchmark evidence."
    Invoke-AndAssert { & (Join-Path "build" "runtime\\benchmarks\\cpu_attention_bench.exe") } "CPU attention benchmark failed."
}

if (Test-Path (Join-Path "build" "runtime\\benchmarks\\cpu_bitnet_bench.exe")) {
    Write-Host "Detected CPU BitNet benchmark binary. Running AVX2 BitNet local benchmark evidence."
    Invoke-AndAssert { & (Join-Path "build" "runtime\\benchmarks\\cpu_bitnet_bench.exe") } "CPU BitNet benchmark failed."
}

if ((Test-Path "build") -and (Get-Command npx -ErrorAction SilentlyContinue)) {
    $cliPath = if ($env:US4_CLI_PATH) {
        $env:US4_CLI_PATH
    } else {
        $rootCandidate = Join-Path (Join-Path (Get-Location) "build") "us4-cli.exe"
        $nestedCandidate = Join-Path (Join-Path (Join-Path (Get-Location) "build") "runtime\cli") "us4-cli.exe"
        if (Test-Path $rootCandidate) { $rootCandidate } else { $nestedCandidate }
    }
    if (Test-Path $cliPath) {
        Write-Host "Detected us4-cli binary. Running CLI Playwright smoke validation."
        Invoke-AndAssert { npx playwright test --project=cli --reporter=list } "Playwright CLI smoke failed."
    } else {
        Write-Host "Build directory found, but us4-cli binary is missing. Skipping CLI Playwright smoke."
    }
}
