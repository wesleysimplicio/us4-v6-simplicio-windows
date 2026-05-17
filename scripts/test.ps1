$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "use-msvc-toolchain.ps1")

Write-Host "Running current repository validation"

npm run test:cli
npm run pack:dry

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
    (Join-Path (Get-Location) "scripts\generate-checksums.ps1"),
    (Join-Path (Get-Location) "scripts\install-msix-smoke.ps1"),
    (Join-Path (Get-Location) "scripts\post-publish-smoke.ps1"),
    (Join-Path (Get-Location) "scripts\preflight-release.ps1"),
    (Join-Path (Get-Location) "scripts\render-project-status.ps1"),
    (Join-Path (Get-Location) "scripts\release-dry-run.ps1"),
    (Join-Path (Get-Location) "scripts\render-planning-status.ps1"),
    (Join-Path (Get-Location) "scripts\render-release-manifest.ps1"),
    (Join-Path (Get-Location) "scripts\render-release-notes.ps1"),
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
        cmake -S . -B build
        cmake --build build -j 8
        Write-Host "Running CTest on the rebuilt runtime artifacts."
        ctest --test-dir build --output-on-failure
    } else {
        Write-Host "Build directory found, but cmake/ctest is not available in PATH. Skipping runtime rebuild and test invocation."
    }
}

if (Test-Path (Join-Path "build" "runtime\\benchmarks\\correctness_diff.exe")) {
    Write-Host "Detected correctness gate binary. Running CPU scalar correctness validation."
    & (Join-Path "build" "runtime\\benchmarks\\correctness_diff.exe")
}

if (Test-Path (Join-Path "build" "runtime\\benchmarks\\hybrid_planner_gate.exe")) {
    Write-Host "Detected hybrid planner gate binary. Running Vulkan/Windows ML correctness validation."
    & (Join-Path "build" "runtime\\benchmarks\\hybrid_planner_gate.exe")
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
        npx playwright test --project=cli --reporter=list
    } else {
        Write-Host "Build directory found, but us4-cli binary is missing. Skipping CLI Playwright smoke."
    }
}
