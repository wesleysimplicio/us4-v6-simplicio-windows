$ErrorActionPreference = "Stop"

$completionScript = Join-Path $PSScriptRoot "completions\us4-cli.ps1"
if (-not (Test-Path $completionScript)) {
    throw "Completion script not found: $completionScript"
}

if (-not (Test-Path $PROFILE)) {
    New-Item -ItemType File -Path $PROFILE -Force | Out-Null
}

$loadLine = ". `"$completionScript`""
$profileContent = Get-Content $PROFILE -Raw
if ($profileContent -notlike "*$loadLine*") {
    Add-Content -Path $PROFILE -Value "`r`n$loadLine`r`n"
}

Write-Host "Installed us4-cli PowerShell completions into $PROFILE"
