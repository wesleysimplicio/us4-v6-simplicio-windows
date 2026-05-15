param(
    [string]$Version = "",
    [string]$PackageIdentifier = "us4-core.US4V6WindowsEdition",
    [string]$PackageName = "US4 V6 Windows Edition",
    [string]$Publisher = "us4-core",
    [string]$PortableUrl = "https://example.invalid/us4-v6-windows-portable.zip",
    [string]$MsixUrl = "https://example.invalid/us4-v6-windows.msix",
    [string]$OutputDir = "packaging\\winget\\manifests"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = (Get-Content package.json -Raw | ConvertFrom-Json).version
}

$templateDir = Join-Path $PSScriptRoot "..\\packaging\\winget\\templates"
$resolvedTemplateDir = (Resolve-Path $templateDir).Path
$resolvedOutputDir = if ([System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir
} else {
    Join-Path (Get-Location) $OutputDir
}

New-Item -ItemType Directory -Path $resolvedOutputDir -Force | Out-Null

$replacements = @{
    "@PACKAGE_IDENTIFIER@" = $PackageIdentifier
    "@PACKAGE_NAME@" = $PackageName
    "@PUBLISHER@" = $Publisher
    "@PACKAGE_VERSION@" = $Version
    "@PORTABLE_URL@" = $PortableUrl
    "@MSIX_URL@" = $MsixUrl
}

Get-ChildItem -Path $resolvedTemplateDir -Filter *.in | ForEach-Object {
    $content = Get-Content $_.FullName -Raw
    foreach ($entry in $replacements.GetEnumerator()) {
        $content = $content.Replace($entry.Key, $entry.Value)
    }

    $outputName = [System.IO.Path]::GetFileNameWithoutExtension($_.Name)
    $outputPath = Join-Path $resolvedOutputDir $outputName
    Set-Content -Path $outputPath -Value $content -Encoding UTF8
}

Write-Host "Rendered winget manifests into $resolvedOutputDir"
