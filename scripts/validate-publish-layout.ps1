param(
    [string]$OutputDir = "dist",
    [string]$ExpectedVersion = "",
    [string]$Format = "text"
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

$issues = New-Object System.Collections.Generic.List[string]
$files = New-Object System.Collections.Generic.List[string]
$directories = New-Object System.Collections.Generic.List[string]

$packageParts = $ExpectedVersion.Split(".")
$expectedMsixVersion = "{0}.{1}.{2}.0" -f $packageParts[0], $packageParts[1], $packageParts[2]
$allowedFiles = @(
    "SHA256SUMS.txt",
    "release-manifest.json",
    "release-notes.md",
    "us4-v6-windows-$ExpectedVersion-portable.zip",
    "us4-v6-windows-$expectedMsixVersion.msix"
)

if (-not (Test-Path $resolvedOutputDir)) {
    [void]$issues.Add("output_dir_missing")
} else {
    foreach ($entry in Get-ChildItem -Path $resolvedOutputDir -Force) {
        if ($entry.PSIsContainer) {
            [void]$directories.Add($entry.Name)
            [void]$issues.Add("unexpected_directory_present")
            continue
        }

        [void]$files.Add($entry.Name)
        if ($entry.Name -notin $allowedFiles) {
            [void]$issues.Add("unexpected_file_present")
        }
    }
}

$hasPortableZip = $files.Contains("us4-v6-windows-$ExpectedVersion-portable.zip")
if (-not $hasPortableZip) {
    [void]$issues.Add("portable_zip_missing")
}

$payload = [ordered]@{
    execution = "validate-publish-layout"
    status = if ($issues.Count -eq 0) { "ready" } else { "blocked" }
    output_dir = $resolvedOutputDir
    expected_version = $ExpectedVersion
    expected_msix_version = $expectedMsixVersion
    files = @($files)
    directories = @($directories)
    allowed_files = @($allowedFiles)
    issue_codes = @($issues | Select-Object -Unique)
}

if ($Format -eq "json") {
    $payload | ConvertTo-Json -Depth 5
} else {
    Write-Host "execution: validate-publish-layout"
    Write-Host "status: $($payload.status)"
    Write-Host "output_dir: $resolvedOutputDir"
    Write-Host "expected_version: $ExpectedVersion"
    Write-Host "expected_msix_version: $expectedMsixVersion"
    Write-Host ("files: " + ($(if ($files.Count -gt 0) { $files -join "," } else { "none" })))
    Write-Host ("directories: " + ($(if ($directories.Count -gt 0) { $directories -join "," } else { "none" })))
    if ($payload.issue_codes.Count -gt 0) {
        Write-Host ("issue_codes: " + ($payload.issue_codes -join ","))
    } else {
        Write-Host "issue_codes: none"
    }
}

if ($payload.status -ne "ready") {
    exit 1
}
