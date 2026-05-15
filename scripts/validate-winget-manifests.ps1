param(
    [string]$ManifestDir = "packaging\\winget\\manifests",
    [string]$ExpectedVersion = "",
    [string]$Format = "text",
    [switch]$RequirePublishableUrls
)

$ErrorActionPreference = "Stop"

if ($Format -notin @("text", "json")) {
    throw "Unknown value for -Format. Use text or json."
}

if ([string]::IsNullOrWhiteSpace($ExpectedVersion)) {
    $ExpectedVersion = (Get-Content package.json -Raw | ConvertFrom-Json).version
}

$resolvedManifestDir = if ([System.IO.Path]::IsPathRooted($ManifestDir)) {
    $ManifestDir
} else {
    Join-Path (Get-Location) $ManifestDir
}

$issues = New-Object System.Collections.Generic.List[string]
$requiredFiles = @("default.yaml", "installer.yaml", "locale.en-US.yaml")
$contents = @{}

if (-not (Test-Path $resolvedManifestDir)) {
    [void]$issues.Add("manifest_dir_missing")
} else {
    foreach ($fileName in $requiredFiles) {
        $path = Join-Path $resolvedManifestDir $fileName
        if (-not (Test-Path $path)) {
            [void]$issues.Add("manifest_file_missing:$fileName")
            continue
        }

        $content = Get-Content $path -Raw
        $contents[$fileName] = $content

        if ($content -match "@[A-Z0-9_]+@") {
            [void]$issues.Add("manifest_contains_placeholders:$fileName")
        }

        if ($content -notmatch ("PackageVersion:\s*" + [regex]::Escape($ExpectedVersion))) {
            [void]$issues.Add("manifest_version_mismatch:$fileName")
        }
    }
}

$installerUrls = @()
if ($contents.ContainsKey("installer.yaml")) {
    $installerMatches = [regex]::Matches($contents["installer.yaml"], "InstallerUrl:\s*(\S+)")
    foreach ($match in $installerMatches) {
        $installerUrls += $match.Groups[1].Value.Trim()
    }

    if ($installerUrls.Count -lt 2) {
        [void]$issues.Add("installer_urls_missing")
    }

    foreach ($url in $installerUrls) {
        $isHttps = $url.StartsWith("https://", [System.StringComparison]::OrdinalIgnoreCase)
        if (-not $isHttps) {
            [void]$issues.Add("installer_url_not_https")
            continue
        }

        if ($RequirePublishableUrls -and $url -match "example\.invalid|placeholder|localhost") {
            [void]$issues.Add("installer_url_not_publishable")
        }
    }
}

$status = if ($issues.Count -eq 0) { "ready" } else { "blocked" }
$payload = [ordered]@{
    execution = "validate-winget-manifests"
    status = $status
    manifest_dir = $resolvedManifestDir
    expected_version = $ExpectedVersion
    require_publishable_urls = [bool]$RequirePublishableUrls
    manifest_files = $requiredFiles
    installer_urls = @($installerUrls)
    issue_codes = @($issues)
}

if ($Format -eq "json") {
    $payload | ConvertTo-Json -Depth 5
} else {
    Write-Host "execution: validate-winget-manifests"
    Write-Host "status: $status"
    Write-Host "manifest_dir: $resolvedManifestDir"
    Write-Host "expected_version: $ExpectedVersion"
    Write-Host "require_publishable_urls: $($RequirePublishableUrls.ToString().ToLowerInvariant())"
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
