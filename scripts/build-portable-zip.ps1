param(
    [string]$BuildDir = "build",
    [string]$OutputDir = "dist"
)

$ErrorActionPreference = "Stop"

$cliPath = Join-Path $BuildDir "runtime\cli\us4-cli.exe"
if (-not (Test-Path $cliPath)) {
    $cliPath = Join-Path $BuildDir "us4-cli.exe"
}
if (-not (Test-Path $cliPath)) {
    throw "Could not find us4-cli.exe under $BuildDir"
}

$version = (Get-Content package.json -Raw | ConvertFrom-Json).version
$stagingDir = Join-Path $OutputDir "us4-portable"
$zipPath = Join-Path $OutputDir "us4-v6-windows-$version-portable.zip"

if (Test-Path $stagingDir) {
    Remove-Item -Recurse -Force $stagingDir
}
New-Item -ItemType Directory -Path $stagingDir -Force | Out-Null
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

Copy-Item $cliPath (Join-Path $stagingDir "us4-cli.exe")
Copy-Item README.md (Join-Path $stagingDir "README.md")
Copy-Item README.pt-BR.md (Join-Path $stagingDir "README.pt-BR.md")
Copy-Item CHANGELOG.md (Join-Path $stagingDir "CHANGELOG.md")

if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}

Compress-Archive -Path (Join-Path $stagingDir "*") -DestinationPath $zipPath
Write-Host "Portable zip created at $zipPath"
