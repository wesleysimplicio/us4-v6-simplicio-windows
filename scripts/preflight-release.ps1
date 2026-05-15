param(
    [string]$BuildDir = "build",
    [string]$Format = "text",
    [switch]$RequireMsix,
    [switch]$RequireSigning
)

$ErrorActionPreference = "Stop"

if ($Format -notin @("text", "json")) {
    throw "Unknown value for -Format. Use text or json."
}

$packageVersion = (Get-Content package.json -Raw | ConvertFrom-Json).version
$cmakeText = Get-Content CMakeLists.txt -Raw
$cmakeMatch = [regex]::Match($cmakeText, 'VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)')
$cmakeVersion = if ($cmakeMatch.Success) { $cmakeMatch.Groups[1].Value } else { "" }
$changelogText = Get-Content CHANGELOG.md -Raw
$changelogHasVersion = $changelogText.Contains("## [$packageVersion]")

$cliPath = Join-Path $BuildDir "runtime\cli\us4-cli.exe"
if (-not (Test-Path $cliPath)) {
    $cliPath = Join-Path $BuildDir "us4-cli.exe"
}

$hasBuild = Test-Path $BuildDir
$hasCli = Test-Path $cliPath
$hasMakeAppx = $null -ne (Get-Command MakeAppx.exe -ErrorAction SilentlyContinue)
$hasSignTool = $null -ne (Get-Command signtool.exe -ErrorAction SilentlyContinue)
$hasSigningPath = -not [string]::IsNullOrWhiteSpace($env:US4_SIGN_CERT_PATH)
$hasSigningBase64 = -not [string]::IsNullOrWhiteSpace($env:US4_SIGN_CERT_BASE64)
$hasSigningPassword = -not [string]::IsNullOrWhiteSpace($env:US4_SIGN_CERT_PASSWORD)
$hasSigningConfig = ($hasSigningPath -or $hasSigningBase64) -and $hasSigningPassword

$issues = New-Object System.Collections.Generic.List[string]

if (-not $cmakeMatch.Success) {
    [void]$issues.Add("cmake_version_missing")
}
if ($cmakeVersion -ne $packageVersion) {
    [void]$issues.Add("version_mismatch")
}
if (-not $changelogHasVersion) {
    [void]$issues.Add("changelog_missing_current_version")
}
if (-not $hasBuild) {
    [void]$issues.Add("build_dir_missing")
}
if (-not $hasCli) {
    [void]$issues.Add("cli_binary_missing")
}
if ($RequireMsix -and -not $hasMakeAppx) {
    [void]$issues.Add("makeappx_missing")
}
if ($RequireSigning -and -not $hasSigningConfig) {
    [void]$issues.Add("signing_config_missing")
}
if ($RequireSigning -and -not $hasSignTool) {
    [void]$issues.Add("signtool_missing")
}

$status = if ($issues.Count -eq 0) { "ready" } else { "blocked" }

$payload = [ordered]@{
    execution = "release-preflight"
    status = $status
    package_version = $packageVersion
    cmake_version = $cmakeVersion
    changelog_has_current_version = $changelogHasVersion
    build_dir = $BuildDir
    cli_path = $cliPath
    has_build_dir = $hasBuild
    has_cli_binary = $hasCli
    has_makeappx = $hasMakeAppx
    has_signtool = $hasSignTool
    has_signing_config = $hasSigningConfig
    require_msix = [bool]$RequireMsix
    require_signing = [bool]$RequireSigning
    issue_codes = @($issues)
}

if ($Format -eq "json") {
    $payload | ConvertTo-Json -Depth 5
} else {
    Write-Host "execution: release-preflight"
    Write-Host "status: $status"
    Write-Host "package_version: $packageVersion"
    Write-Host "cmake_version: $cmakeVersion"
    Write-Host "changelog_has_current_version: $($changelogHasVersion.ToString().ToLowerInvariant())"
    Write-Host "build_dir: $BuildDir"
    Write-Host "cli_path: $cliPath"
    Write-Host "has_build_dir: $($hasBuild.ToString().ToLowerInvariant())"
    Write-Host "has_cli_binary: $($hasCli.ToString().ToLowerInvariant())"
    Write-Host "has_makeappx: $($hasMakeAppx.ToString().ToLowerInvariant())"
    Write-Host "has_signtool: $($hasSignTool.ToString().ToLowerInvariant())"
    Write-Host "has_signing_config: $($hasSigningConfig.ToString().ToLowerInvariant())"
    if ($issues.Count -gt 0) {
        Write-Host ("issue_codes: " + ($issues -join ","))
    } else {
        Write-Host "issue_codes: none"
    }
}

if ($status -ne "ready") {
    exit 1
}
