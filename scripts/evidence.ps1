$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "use-msvc-toolchain.ps1")

function Test-CommandAvailable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Get-CliPath {
    if ($env:US4_CLI_PATH) {
        return $env:US4_CLI_PATH
    }

    $rootCandidate = Join-Path (Join-Path (Get-Location) "build") "us4-cli.exe"
    if (Test-Path $rootCandidate) {
        return $rootCandidate
    }

    return Join-Path (Join-Path (Join-Path (Get-Location) "build") "runtime\cli") "us4-cli.exe"
}

function Show-BuildGuidance {
    [void](Enable-MsvcToolchain)
    $cliPath = Get-CliPath
    Write-Host "CLI evidence is blocked because the executable was not found."
    Write-Host "Expected binary: $cliPath"
    Write-Host "A skipped Playwright run does not satisfy the CLI/UX evidence requirement."
    Write-Host ""
    Write-Host "Toolchain snapshot:"
    Write-Host "  cmake     : $([bool](Test-CommandAvailable 'cmake'))"
    Write-Host "  ninja     : $([bool](Test-CommandAvailable 'ninja'))"
    Write-Host "  cl        : $([bool](Test-CommandAvailable 'cl'))"
    Write-Host "  clang-cl  : $([bool](Test-CommandAvailable 'clang-cl'))"
    Write-Host "  clang++   : $([bool](Test-CommandAvailable 'clang++'))"
    Write-Host ""
    Write-Host "Build us4-cli first, for example:"
    Write-Host 'cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release'
    Write-Host 'cmake --build build --target us4-cli'
}

[void](Enable-MsvcToolchain)
$cliPath = Get-CliPath

if (-not (Test-Path $cliPath)) {
    Show-BuildGuidance
    exit 1
}

Write-Host "Capturing Playwright evidence for us4-cli"
$env:PLAYWRIGHT_JSON_OUTPUT_NAME = "test-results/results.json"
npx playwright test --project=cli --reporter=list,html,json

if (-not (Test-Path "playwright-report/index.html")) {
    Write-Host "Playwright HTML report was not generated."
    exit 1
}

if (-not (Test-Path "test-results/results.json")) {
    Write-Host "Playwright JSON results were not generated."
    exit 1
}

$summaryScript = @'
const fs = require("node:fs");
const report = JSON.parse(fs.readFileSync("test-results/results.json", "utf8"));

function walkSuite(suite, totals) {
  for (const spec of suite.specs ?? []) {
    for (const test of spec.tests ?? []) {
      totals.total += 1;
      const status = test.status ?? "unknown";
      totals[status] = (totals[status] ?? 0) + 1;
    }
  }

  for (const child of suite.suites ?? []) {
    walkSuite(child, totals);
  }
}

const totals = { total: 0, expected: 0, skipped: 0, unexpected: 0, flaky: 0, unknown: 0 };
for (const suite of report.suites ?? []) {
  walkSuite(suite, totals);
}

console.log(JSON.stringify(totals));

if (!totals.total || totals.total === totals.skipped) {
  process.exit(2);
}
'@

$summaryScriptPath = Join-Path $env:TEMP "us4-playwright-summary.cjs"
Set-Content -Path $summaryScriptPath -Value $summaryScript -Encoding ASCII
$summaryJson = node $summaryScriptPath
$nodeExitCode = $LASTEXITCODE
Remove-Item $summaryScriptPath -ErrorAction SilentlyContinue
if ($nodeExitCode -eq 2) {
    Write-Host "Playwright only produced skipped tests. Evidence is not complete."
    exit 1
}
if ($nodeExitCode -ne 0) {
    exit $nodeExitCode
}

Write-Host "Playwright evidence summary: $summaryJson"
