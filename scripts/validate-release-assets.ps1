param(
    [string]$OutputDir = "dist",
    [string]$ManifestDir = "packaging\\winget\\manifests",
    [string]$ExpectedVersion = "",
    [string]$Format = "text",
    [switch]$RequireMsix
)

$ErrorActionPreference = "Stop"

if ($Format -notin @("text", "json")) {
    throw "Unknown value for -Format. Use text or json."
}

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

$issues = New-Object System.Collections.Generic.List[string]
$artifacts = New-Object System.Collections.Generic.List[string]
$installerUrls = New-Object System.Collections.Generic.List[string]

$packageParts = $ExpectedVersion.Split(".")
$expectedMsixVersion = "{0}.{1}.{2}.0" -f $packageParts[0], $packageParts[1], $packageParts[2]
$expectedZipName = "us4-v6-windows-$ExpectedVersion-portable.zip"
$expectedMsixName = "us4-v6-windows-$expectedMsixVersion.msix"
$checksumsName = "SHA256SUMS.txt"
$checksumsPath = Join-Path $resolvedOutputDir $checksumsName

if (-not (Test-Path $resolvedOutputDir)) {
    [void]$issues.Add("output_dir_missing")
} else {
    $zipPath = Join-Path $resolvedOutputDir $expectedZipName
    if (Test-Path $zipPath) {
        [void]$artifacts.Add($expectedZipName)
    } else {
        [void]$issues.Add("portable_zip_missing")
    }

    $msixPath = Join-Path $resolvedOutputDir $expectedMsixName
    if (Test-Path $msixPath) {
        [void]$artifacts.Add($expectedMsixName)
    } elseif ($RequireMsix) {
        [void]$issues.Add("msix_missing")
    }

    if (Test-Path $checksumsPath) {
        [void]$artifacts.Add($checksumsName)
        $checksumLines = Get-Content $checksumsPath | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
        if ($checksumLines.Count -eq 0) {
            [void]$issues.Add("checksums_empty")
        }

        $hasZipChecksum = $false
        $hasMsixChecksum = $false
        foreach ($line in $checksumLines) {
            if ($line -notmatch '^[A-Fa-f0-9]{64}\s+\*?.+$') {
                [void]$issues.Add("checksum_line_invalid")
                continue
            }

            if ($line -match [regex]::Escape($expectedZipName)) {
                $hasZipChecksum = $true
            }
            if ($line -match [regex]::Escape($expectedMsixName)) {
                $hasMsixChecksum = $true
            }
        }

        if (-not $hasZipChecksum) {
            [void]$issues.Add("checksum_missing_portable_zip")
        }
        if ($RequireMsix -and -not $hasMsixChecksum) {
            [void]$issues.Add("checksum_missing_msix")
        }
    } else {
        [void]$issues.Add("checksums_missing")
    }
}

if (-not (Test-Path $resolvedManifestDir)) {
    [void]$issues.Add("manifest_dir_missing")
} else {
    $installerManifestPath = Join-Path $resolvedManifestDir "installer.yaml"
    if (-not (Test-Path $installerManifestPath)) {
        [void]$issues.Add("installer_manifest_missing")
    } else {
        $installerManifest = Get-Content $installerManifestPath -Raw
        $installerMatches = [regex]::Matches($installerManifest, "InstallerUrl:\s*(\S+)")
        foreach ($match in $installerMatches) {
            $url = $match.Groups[1].Value.Trim()
            [void]$installerUrls.Add($url)
        }

        $zipUrlAligned = $false
        $msixUrlAligned = $false
        foreach ($url in $installerUrls) {
            if ($url.EndsWith($expectedZipName, [System.StringComparison]::OrdinalIgnoreCase)) {
                $zipUrlAligned = $true
            }
            if ($url.EndsWith($expectedMsixName, [System.StringComparison]::OrdinalIgnoreCase)) {
                $msixUrlAligned = $true
            }
        }

        if (-not $zipUrlAligned) {
            [void]$issues.Add("winget_portable_url_mismatch")
        }
        if ($RequireMsix -and -not $msixUrlAligned) {
            [void]$issues.Add("winget_msix_url_mismatch")
        }
    }
}

$status = if ($issues.Count -eq 0) { "ready" } else { "blocked" }
$payload = [ordered]@{
    execution = "validate-release-assets"
    status = $status
    output_dir = $resolvedOutputDir
    manifest_dir = $resolvedManifestDir
    expected_version = $ExpectedVersion
    expected_msix_version = $expectedMsixVersion
    require_msix = [bool]$RequireMsix
    artifacts = @($artifacts)
    installer_urls = @($installerUrls)
    issue_codes = @($issues)
}

if ($Format -eq "json") {
    $payload | ConvertTo-Json -Depth 5
} else {
    Write-Host "execution: validate-release-assets"
    Write-Host "status: $status"
    Write-Host "output_dir: $resolvedOutputDir"
    Write-Host "manifest_dir: $resolvedManifestDir"
    Write-Host "expected_version: $ExpectedVersion"
    Write-Host "expected_msix_version: $expectedMsixVersion"
    Write-Host "require_msix: $($RequireMsix.ToString().ToLowerInvariant())"
    if ($artifacts.Count -gt 0) {
        Write-Host ("artifacts: " + ($artifacts -join ","))
    } else {
        Write-Host "artifacts: none"
    }
    if ($installerUrls.Count -gt 0) {
        Write-Host ("installer_urls: " + ($installerUrls -join ","))
    } else {
        Write-Host "installer_urls: none"
    }
    if ($issues.Count -gt 0) {
        Write-Host ("issue_codes: " + ($issues -join ","))
    } else {
        Write-Host "issue_codes: none"
    }
}

if ($status -ne "ready") {
    exit 1
}
