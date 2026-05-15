$ErrorActionPreference = "Stop"

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

if ((Test-Path "CMakeLists.txt") -and (Test-Path "build")) {
    if (Get-Command ctest -ErrorAction SilentlyContinue) {
        Write-Host "Detected runtime scaffold/build. Running CTest as additional validation."
        ctest --test-dir build --output-on-failure
    } else {
        Write-Host "Build directory found, but ctest is not available in PATH. Skipping runtime test invocation."
    }
}

if ((Test-Path "build") -and (Get-Command npx -ErrorAction SilentlyContinue)) {
    $cliPath = if ($env:US4_CLI_PATH) { $env:US4_CLI_PATH } else { Join-Path (Join-Path (Get-Location) "build") "us4-cli.exe" }
    if (Test-Path $cliPath) {
        Write-Host "Detected us4-cli binary. Running CLI Playwright smoke validation."
        npx playwright test --project=cli --reporter=list
    } else {
        Write-Host "Build directory found, but us4-cli binary is missing. Skipping CLI Playwright smoke."
    }
}
