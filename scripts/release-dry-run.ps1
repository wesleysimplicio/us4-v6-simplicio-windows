param(
    [string]$BuildDir = "build",
    [string]$OutputDir = "dist",
    [string]$ManifestDir = "",
    [string]$Version = "",
    [string]$Tag = "",
    [string]$Repository = "wesleysimplicio/us4-v6-simplicio-windows",
    [string]$Format = "text",
    [string]$WorkingDir = "",
    [string]$DevCertificatePassword = "",
    [switch]$IncludeDevMsixSmoke
)

$ErrorActionPreference = "Stop"

if ($Format -notin @("text", "json")) {
    throw "Unknown value for -Format. Use text or json."
}

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = (Get-Content package.json -Raw | ConvertFrom-Json).version
}

if ([string]::IsNullOrWhiteSpace($Tag)) {
    $Tag = "v$Version"
}

if ($IncludeDevMsixSmoke -and [string]::IsNullOrWhiteSpace($DevCertificatePassword)) {
    throw "DevCertificatePassword is required when IncludeDevMsixSmoke is enabled."
}

$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir
} else {
    Join-Path (Get-Location) $BuildDir
}

$resolvedOutputDir = if ([System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir
} else {
    Join-Path (Get-Location) $OutputDir
}

if ([string]::IsNullOrWhiteSpace($WorkingDir)) {
    $WorkingDir = Join-Path ([System.IO.Path]::GetTempPath()) ("us4-release-dry-run-" + [System.Guid]::NewGuid().ToString("N"))
} elseif (-not [System.IO.Path]::IsPathRooted($WorkingDir)) {
    $WorkingDir = Join-Path (Get-Location) $WorkingDir
}

$resolvedManifestDir = if ([string]::IsNullOrWhiteSpace($ManifestDir)) {
    Join-Path $WorkingDir "winget-manifests"
} elseif ([System.IO.Path]::IsPathRooted($ManifestDir)) {
    $ManifestDir
} else {
    Join-Path (Get-Location) $ManifestDir
}

if (Test-Path $resolvedOutputDir) {
    Remove-Item -Recurse -Force $resolvedOutputDir
}
New-Item -ItemType Directory -Path $resolvedOutputDir -Force | Out-Null

if (Test-Path $WorkingDir) {
    Remove-Item -Recurse -Force $WorkingDir
}
New-Item -ItemType Directory -Path $WorkingDir -Force | Out-Null

$parts = $Version.Split('.')
if ($parts.Count -lt 3) {
    throw "Version must have three segments."
}
$msixVersion = "{0}.{1}.{2}.0" -f $parts[0], $parts[1], $parts[2]
$portableUrl = "https://github.com/$Repository/releases/download/$Tag/us4-v6-windows-$Version-portable.zip"
$msixUrl = "https://github.com/$Repository/releases/download/$Tag/us4-v6-windows-$msixVersion.msix"
$devMsixSmokeScriptPath = Join-Path (Get-Location) "scripts\dev-msix-smoke.ps1"

$steps = New-Object System.Collections.Generic.List[object]
$issues = New-Object System.Collections.Generic.List[string]

function Add-Step {
    param(
        [string]$Name,
        [string]$Status,
        [string]$Detail = ""
    )

    [void]$steps.Add([pscustomobject][ordered]@{
        name = $Name
        status = $Status
        detail = $Detail
    })
}

function Invoke-Step {
    param(
        [string]$Name,
        [scriptblock]$Action
    )

    try {
        & $Action
        Add-Step -Name $Name -Status "passed"
    } catch {
        [void]$issues.Add(("{0}:{1}" -f $Name, $_.Exception.Message))
        Add-Step -Name $Name -Status "failed" -Detail $_.Exception.Message
        throw
    }
}

Invoke-Step "preflight" {
    & (Join-Path (Get-Location) "scripts\preflight-release.ps1") `
        -BuildDir $resolvedBuildDir `
        -Format json *> $null
}

$tagValidation = (& (Join-Path (Get-Location) "scripts\validate-release-tag.ps1") `
    -Tag $Tag `
    -ExpectedVersion $Version `
    -Format json) | ConvertFrom-Json
$tagValidationPassed = $tagValidation.status -eq "ready"
if ($tagValidationPassed) {
    Add-Step -Name "validate-release-tag" -Status "passed"
} else {
    [void]$issues.Add("validate-release-tag:Release tag validation failed.")
    Add-Step -Name "validate-release-tag" -Status "failed" -Detail "Release tag validation failed."
}

if ($tagValidationPassed) {
    Invoke-Step "build-portable-zip" {
        & (Join-Path (Get-Location) "scripts\build-portable-zip.ps1") `
            -BuildDir $resolvedBuildDir `
            -OutputDir $resolvedOutputDir `
            -WorkingDir (Join-Path $WorkingDir "portable-package") *> $null
    }
}

$msixBuilt = $false
$devMsixPreflight = (& $devMsixSmokeScriptPath `
    -BuildDir $resolvedBuildDir `
    -OutputDir $resolvedOutputDir `
    -WorkingDir (Join-Path $WorkingDir "dev-msix-smoke") `
    -PreflightOnly `
    -Format json) | ConvertFrom-Json

if ($tagValidationPassed) {
    try {
        & (Join-Path (Get-Location) "scripts\build-msix.ps1") `
            -BuildDir $resolvedBuildDir `
            -OutputDir $resolvedOutputDir `
            -WorkingDir (Join-Path $WorkingDir "msix-package") *> $null
        $msixBuilt = @(Get-ChildItem -Path $resolvedOutputDir -Filter *.msix -ErrorAction SilentlyContinue).Count -gt 0
        if ($msixBuilt) {
            Add-Step -Name "build-msix" -Status "passed"
        } else {
            Add-Step -Name "build-msix" -Status "skipped" -Detail "Packaging tools unavailable or MSIX output missing."
        }
    } catch {
        Add-Step -Name "build-msix" -Status "skipped" -Detail $_.Exception.Message
    }

    Invoke-Step "generate-checksums" {
        & (Join-Path (Get-Location) "scripts\generate-checksums.ps1") `
            -OutputDir $resolvedOutputDir *> $null
    }

    Invoke-Step "render-winget-manifests" {
        & (Join-Path (Get-Location) "scripts\render-winget-manifests.ps1") `
            -Version $Version `
            -PortableUrl $portableUrl `
            -MsixUrl $msixUrl `
            -OutputDir $resolvedManifestDir *> $null
    }

    Invoke-Step "validate-winget-manifests" {
        & (Join-Path (Get-Location) "scripts\validate-winget-manifests.ps1") `
            -ManifestDir $resolvedManifestDir `
            -ExpectedVersion $Version `
            -RequirePublishableUrls `
            -Format json *> $null
    }

    Invoke-Step "validate-release-assets" {
        if ($msixBuilt) {
            & (Join-Path (Get-Location) "scripts\validate-release-assets.ps1") `
                -OutputDir $resolvedOutputDir `
                -ManifestDir $resolvedManifestDir `
                -ExpectedVersion $Version `
                -RequireMsix *> $null
        } else {
            & (Join-Path (Get-Location) "scripts\validate-release-assets.ps1") `
                -OutputDir $resolvedOutputDir `
                -ManifestDir $resolvedManifestDir `
                -ExpectedVersion $Version *> $null
        }
    }

    Invoke-Step "render-release-manifest" {
        & (Join-Path (Get-Location) "scripts\render-release-manifest.ps1") `
            -OutputDir $resolvedOutputDir `
            -ManifestDir $resolvedManifestDir `
            -ExpectedVersion $Version *> $null
    }

    Invoke-Step "render-release-notes" {
        & (Join-Path (Get-Location) "scripts\render-release-notes.ps1") `
            -Version $Version `
            -ChangelogPath (Join-Path (Get-Location) "CHANGELOG.md") `
            -ReleaseManifestPath (Join-Path $resolvedOutputDir "release-manifest.json") `
            -OutputPath (Join-Path $resolvedOutputDir "release-notes.md") *> $null
    }

    Invoke-Step "validate-publish-layout" {
        & (Join-Path (Get-Location) "scripts\validate-publish-layout.ps1") `
            -OutputDir $resolvedOutputDir `
            -ExpectedVersion $Version `
            -Format json *> $null
    }

    Invoke-Step "portable-smoke" {
        $portableArtifact = Get-ChildItem -Path $resolvedOutputDir -Filter *.zip | Select-Object -First 1
        if (-not $portableArtifact) {
            throw "Portable artifact not found after packaging."
        }

        & (Join-Path (Get-Location) "scripts\post-publish-smoke.ps1") `
            -ArtifactPath $portableArtifact.FullName `
            -WorkingDir (Join-Path $WorkingDir "portable-smoke") *> $null
    }

    if ($IncludeDevMsixSmoke) {
        if ($devMsixPreflight.status -eq "ready") {
            Invoke-Step "dev-msix-smoke" {
                & $devMsixSmokeScriptPath `
                    -BuildDir $resolvedBuildDir `
                    -OutputDir $resolvedOutputDir `
                    -WorkingDir (Join-Path $WorkingDir "dev-msix-smoke") `
                    -CertificatePassword $DevCertificatePassword `
                    -Format json *> $null
            }
        } else {
            Add-Step -Name "dev-msix-smoke" -Status "skipped" -Detail "Local SDK or signing tooling is unavailable on this host."
        }
    } elseif ($devMsixPreflight.status -eq "ready") {
        Add-Step -Name "dev-msix-smoke" -Status "skipped" -Detail "Opt-in not requested."
    } else {
        Add-Step -Name "dev-msix-smoke" -Status "skipped" -Detail "Local SDK or signing tooling is unavailable on this host."
    }
} else {
    foreach ($stepName in @(
        "build-portable-zip",
        "build-msix",
        "generate-checksums",
        "render-winget-manifests",
        "validate-winget-manifests",
        "validate-release-assets",
        "render-release-manifest",
        "render-release-notes",
        "validate-publish-layout",
        "portable-smoke",
        "dev-msix-smoke"
    )) {
        Add-Step -Name $stepName -Status "skipped" -Detail "Skipped because release tag validation failed."
    }
}

$artifactNames = @(
    Get-ChildItem -Path $resolvedOutputDir -File | ForEach-Object { $_.Name }
)

$status = if ($issues.Count -eq 0) { "ready" } else { "blocked" }
$payload = [pscustomobject][ordered]@{
    execution = "release-dry-run"
    status = $status
    version = $Version
    tag = $Tag
    repository = $Repository
    build_dir = $resolvedBuildDir
    output_dir = $resolvedOutputDir
    manifest_dir = $resolvedManifestDir
    working_dir = $WorkingDir
    portable_url = $portableUrl
    msix_url = $msixUrl
    msix_built = $msixBuilt
    dev_msix_requested = [bool]$IncludeDevMsixSmoke
    dev_msix_preflight = $devMsixPreflight
    artifact_names = $artifactNames
    steps = @($steps.ToArray())
    issue_codes = @($issues)
}

if ($Format -eq "json") {
    $payload | ConvertTo-Json -Depth 6
} else {
    Write-Host "execution: release-dry-run"
    Write-Host "status: $status"
    Write-Host "version: $Version"
    Write-Host "tag: $Tag"
    Write-Host "repository: $Repository"
    Write-Host "output_dir: $resolvedOutputDir"
    Write-Host "manifest_dir: $resolvedManifestDir"
    Write-Host "portable_url: $portableUrl"
    Write-Host "msix_url: $msixUrl"
    Write-Host "msix_built: $($msixBuilt.ToString().ToLowerInvariant())"
    Write-Host "dev_msix_requested: $($IncludeDevMsixSmoke.ToString().ToLowerInvariant())"
    Write-Host "dev_msix_preflight: $($devMsixPreflight.status)"
    Write-Host ("artifact_names: " + ($artifactNames -join ","))
    foreach ($step in $steps) {
        if ([string]::IsNullOrWhiteSpace($step.detail)) {
            Write-Host ("step: {0} => {1}" -f $step.name, $step.status)
        } else {
            Write-Host ("step: {0} => {1} ({2})" -f $step.name, $step.status, $step.detail)
        }
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
