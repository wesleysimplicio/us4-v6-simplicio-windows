param(
    [string]$BuildDir = "build",
    [string]$OutputDir = "dist",
    [string]$MsixVersion = "",
    [string]$WorkingDir = ""
)

$ErrorActionPreference = "Stop"

$makeAppx = Get-Command MakeAppx.exe -ErrorAction SilentlyContinue
if (-not $makeAppx) {
    throw "MakeAppx.exe not found. Install Windows SDK App Packaging tools before building MSIX."
}

$cliPath = Join-Path $BuildDir "runtime\cli\us4-cli.exe"
if (-not (Test-Path $cliPath)) {
    $cliPath = Join-Path $BuildDir "us4-cli.exe"
}
if (-not (Test-Path $cliPath)) {
    throw "Could not find us4-cli.exe under $BuildDir"
}

if ([string]::IsNullOrWhiteSpace($MsixVersion)) {
    $packageVersion = (Get-Content package.json -Raw | ConvertFrom-Json).version
    $versionParts = $packageVersion.Split(".")
    $MsixVersion = "{0}.{1}.{2}.0" -f $versionParts[0], $versionParts[1], $versionParts[2]
}

$manifestTemplate = Join-Path "packaging\msix" "AppxManifest.xml.in"
if ([string]::IsNullOrWhiteSpace($WorkingDir)) {
    $WorkingDir = Join-Path ([System.IO.Path]::GetTempPath()) ("us4-msix-" + [System.Guid]::NewGuid().ToString("N"))
}
$stagingDir = Join-Path $WorkingDir "msix-staging"
$assetsDir = Join-Path $stagingDir "Assets"
$manifestPath = Join-Path $stagingDir "AppxManifest.xml"
$packagePath = Join-Path $OutputDir "us4-v6-windows-$MsixVersion.msix"

if (Test-Path $stagingDir) {
    Remove-Item -Recurse -Force $stagingDir
}
New-Item -ItemType Directory -Path $assetsDir -Force | Out-Null
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

Copy-Item $cliPath (Join-Path $stagingDir "us4-cli.exe")
@("Square44x44Logo.png", "Square150x150Logo.png", "StoreLogo.png") | ForEach-Object {
    New-Item -ItemType File -Path (Join-Path $assetsDir $_) -Force | Out-Null
}

$manifest = (Get-Content $manifestTemplate -Raw).Replace("@US4_MSIX_VERSION@", $MsixVersion)
Set-Content -Path $manifestPath -Value $manifest -Encoding UTF8

if (Test-Path $packagePath) {
    Remove-Item -Force $packagePath
}

& $makeAppx.Source pack /d $stagingDir /p $packagePath /o
Write-Host "Unsigned MSIX created at $packagePath"
