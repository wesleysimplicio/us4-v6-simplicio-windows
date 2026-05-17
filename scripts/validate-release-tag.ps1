param(
    [string]$Tag = "",
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

if ([string]::IsNullOrWhiteSpace($Tag)) {
    $Tag = $env:GITHUB_REF_NAME
}

$issues = New-Object System.Collections.Generic.List[string]
$normalizedTag = $Tag
if (-not [string]::IsNullOrWhiteSpace($normalizedTag) -and $normalizedTag.StartsWith("v")) {
    $normalizedTag = $normalizedTag.Substring(1)
}

if ([string]::IsNullOrWhiteSpace($Tag)) {
    [void]$issues.Add("tag_missing")
} elseif ($normalizedTag -ne $ExpectedVersion) {
    [void]$issues.Add("tag_version_mismatch")
}

$status = if ($issues.Count -eq 0) { "ready" } else { "blocked" }
$payload = [ordered]@{
    execution = "validate-release-tag"
    status = $status
    tag = $Tag
    normalized_tag = $normalizedTag
    expected_version = $ExpectedVersion
    issue_codes = @($issues)
}

if ($Format -eq "json") {
    $payload | ConvertTo-Json -Depth 5
} else {
    Write-Host "execution: validate-release-tag"
    Write-Host "status: $status"
    Write-Host "tag: $Tag"
    Write-Host "normalized_tag: $normalizedTag"
    Write-Host "expected_version: $ExpectedVersion"
    if ($issues.Count -gt 0) {
        Write-Host ("issue_codes: " + ($issues -join ","))
    } else {
        Write-Host "issue_codes: none"
    }
}

if ($status -ne "ready") {
    exit 1
}
