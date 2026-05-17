param(
    [string]$Version = "",
    [string]$ChangelogPath = "CHANGELOG.md",
    [string]$ReleaseManifestPath = "dist\\release-manifest.json",
    [string]$OutputPath = "dist\\release-notes.md"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = (Get-Content package.json -Raw | ConvertFrom-Json).version
}

$resolvedChangelogPath = if ([System.IO.Path]::IsPathRooted($ChangelogPath)) {
    $ChangelogPath
} else {
    Join-Path (Get-Location) $ChangelogPath
}

$resolvedManifestPath = if ([System.IO.Path]::IsPathRooted($ReleaseManifestPath)) {
    $ReleaseManifestPath
} else {
    Join-Path (Get-Location) $ReleaseManifestPath
}

$resolvedOutputPath = if ([System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath
} else {
    Join-Path (Get-Location) $OutputPath
}

if (-not (Test-Path $resolvedChangelogPath)) {
    throw "Changelog not found: $resolvedChangelogPath"
}

$changelogText = Get-Content $resolvedChangelogPath -Raw
$pattern = '(?ms)^## \[' + [regex]::Escape($Version) + '\] - (?<date>[^\r\n]+)\r?\n(?<body>.*?)(?=^## \[|\z)'
$match = [regex]::Match($changelogText, $pattern)
if (-not $match.Success) {
    throw "Could not find changelog section for version $Version"
}

$releaseDate = $match.Groups['date'].Value.Trim()
$releaseBody = $match.Groups['body'].Value.Trim()

$artifactSection = ""
if (Test-Path $resolvedManifestPath) {
    $manifest = Get-Content $resolvedManifestPath -Raw | ConvertFrom-Json
    $lines = New-Object System.Collections.Generic.List[string]
    [void]$lines.Add("## Artifacts")
    [void]$lines.Add("")
    [void]$lines.Add("| Name | Type | SHA256 | URL |")
    [void]$lines.Add("|---|---|---|---|")
    foreach ($artifact in $manifest.artifacts) {
        $url = if ($artifact.url) { $artifact.url } else { "-" }
        [void]$lines.Add("| $($artifact.name) | $($artifact.type) | `$($artifact.sha256)` | $url |")
    }
    $artifactSection = ($lines -join [Environment]::NewLine)
}

$noteLines = New-Object System.Collections.Generic.List[string]
[void]$noteLines.Add("# Release $Version")
[void]$noteLines.Add("")
[void]$noteLines.Add("- Date: $releaseDate")
[void]$noteLines.Add("")
[void]$noteLines.Add("## Highlights")
[void]$noteLines.Add("")
[void]$noteLines.Add($releaseBody)

if (-not [string]::IsNullOrWhiteSpace($artifactSection)) {
    [void]$noteLines.Add("")
    [void]$noteLines.Add($artifactSection)
}

$directory = Split-Path -Parent $resolvedOutputPath
if (-not [string]::IsNullOrWhiteSpace($directory) -and -not (Test-Path $directory)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}

$content = $noteLines -join [Environment]::NewLine
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($resolvedOutputPath, $content, $utf8NoBom)
Write-Host "Release notes written to $resolvedOutputPath"
