param(
    [string]$OutputDir = "dist",
    [string]$ManifestDir = "packaging\\winget\\manifests",
    [string]$ExpectedVersion = "",
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ExpectedVersion)) {
    $ExpectedVersion = (Get-Content package.json -Raw | ConvertFrom-Json).version
}

$resolvedOutputDir = if ([System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir
} else {
    Join-Path (Get-Location) $OutputDir
}

$resolvedManifestDir = if ([System.IO.Path]::IsPathRooted($ManifestDir)) {
    $ManifestDir
} else {
    Join-Path (Get-Location) $ManifestDir
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $resolvedOutputDir "release-manifest.json"
} elseif (-not [System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path (Get-Location) $OutputPath
}

if (-not (Test-Path $resolvedOutputDir)) {
    throw "Output directory not found: $resolvedOutputDir"
}

$checksumsPath = Join-Path $resolvedOutputDir "SHA256SUMS.txt"
if (-not (Test-Path $checksumsPath)) {
    throw "Checksums file not found: $checksumsPath"
}

$checksumMap = @{}
foreach ($line in (Get-Content $checksumsPath | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })) {
    $match = [regex]::Match($line, '^([A-Fa-f0-9]{64})\s+\*?(.+)$')
    if (-not $match.Success) {
        throw "Invalid checksum line: $line"
    }

    $checksumMap[$match.Groups[2].Value.Trim()] = $match.Groups[1].Value.ToLowerInvariant()
}

$installerManifestPath = Join-Path $resolvedManifestDir "installer.yaml"
if (-not (Test-Path $installerManifestPath)) {
    throw "Installer manifest not found: $installerManifestPath"
}

$portableUrl = $null
$msixUrl = $null
$currentInstallerType = ""
foreach ($line in Get-Content $installerManifestPath) {
    if ($line -match 'InstallerType:\s*(.+)$') {
        $currentInstallerType = $matches[1].Trim()
        continue
    }

    if ($line -match 'InstallerUrl:\s*(.+)$') {
        $url = $matches[1].Trim()
        if ($currentInstallerType -eq "msix") {
            $msixUrl = $url
        } elseif (-not $portableUrl) {
            $portableUrl = $url
        }
    }
}

$artifacts = New-Object System.Collections.ArrayList
Get-ChildItem -Path $resolvedOutputDir -File | Where-Object {
    $_.Name -ne "SHA256SUMS.txt" -and $_.Name -ne "release-manifest.json"
} | ForEach-Object {
    $artifactName = $_.Name
    if (-not $checksumMap.ContainsKey($artifactName)) {
        throw "Checksum entry missing for artifact: $artifactName"
    }

    $artifactType = switch ($_.Extension.ToLowerInvariant()) {
        ".zip" { "portable-zip" }
        ".msix" { "msix" }
        default { "other" }
    }

    $artifactUrl = $null
    if ($artifactType -eq "portable-zip") {
        $artifactUrl = $portableUrl
    } elseif ($artifactType -eq "msix") {
        $artifactUrl = $msixUrl
    }

    [void]$artifacts.Add([pscustomobject][ordered]@{
        name = $artifactName
        path = $_.FullName
        type = $artifactType
        sha256 = $checksumMap[$artifactName]
        url = $artifactUrl
    })
}

$payload = [pscustomobject][ordered]@{
    version = $ExpectedVersion
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    checksums_path = $checksumsPath
    installer_urls = [pscustomobject][ordered]@{
        portable = $portableUrl
        msix = $msixUrl
    }
    artifacts = @($artifacts.ToArray())
}

$json = $payload | ConvertTo-Json -Depth 6
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($OutputPath, $json, $utf8NoBom)
Write-Host "Release manifest written to $OutputPath"
