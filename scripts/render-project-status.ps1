param(
    [string]$BuildDir = "build",
    [string]$SprintsDir = ".specs\\sprints",
    [string]$OutputDir = "dist",
    [string]$ManifestDir = "",
    [string]$PlaywrightReportDir = "playwright-report",
    [string]$TestResultsDir = "test-results",
    [string]$Format = "json",
    [string]$OutputPath = "",
    [switch]$RequireEvidence,
    [switch]$IncludeReleaseDryRun
)

$ErrorActionPreference = "Stop"

if ($Format -notin @("json", "markdown")) {
    throw "Unknown value for -Format. Use json or markdown."
}

function Resolve-LocalPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return $PathValue
    }

    return Join-Path (Get-Location) $PathValue
}

function Add-IssueCode {
    param(
        [System.Collections.Generic.List[string]]$List,
        [string]$Code
    )

    if (-not $List.Contains($Code)) {
        [void]$List.Add($Code)
    }
}

$resolvedBuildDir = Resolve-LocalPath $BuildDir
$resolvedSprintsDir = Resolve-LocalPath $SprintsDir
$resolvedOutputDir = Resolve-LocalPath $OutputDir
$resolvedPlaywrightReportDir = Resolve-LocalPath $PlaywrightReportDir
$resolvedTestResultsDir = Resolve-LocalPath $TestResultsDir
$resolvedManifestDir = if ([string]::IsNullOrWhiteSpace($ManifestDir)) {
    Join-Path ([System.IO.Path]::GetTempPath()) ("us4-project-status-" + [System.Guid]::NewGuid().ToString("N") + "\winget-manifests")
} else {
    Resolve-LocalPath $ManifestDir
}

$planningScriptPath = Join-Path (Get-Location) "scripts\render-planning-status.ps1"
$preflightScriptPath = Join-Path (Get-Location) "scripts\preflight-release.ps1"
$releaseDryRunScriptPath = Join-Path (Get-Location) "scripts\release-dry-run.ps1"

$planning = (& $planningScriptPath -SprintsDir $resolvedSprintsDir -Format json) | ConvertFrom-Json

$releasePreflight = (& $preflightScriptPath -BuildDir $resolvedBuildDir -Format json) | ConvertFrom-Json

$releaseDryRun = $null
if ($IncludeReleaseDryRun) {
    $releaseDryRun = (& $releaseDryRunScriptPath `
        -BuildDir $resolvedBuildDir `
        -OutputDir $resolvedOutputDir `
        -ManifestDir $resolvedManifestDir `
        -Format json) | ConvertFrom-Json
}

$sprint12Path = Join-Path (Join-Path $resolvedSprintsDir "sprint-12") "SPRINT.md"
$remainingSprint12Tasks = @()
if (Test-Path $sprint12Path) {
    foreach ($line in Get-Content $sprint12Path) {
        if ($line -match '^- \[ \] (T[0-9]+\.[0-9]+) - (.+)$') {
            $remainingSprint12Tasks += [pscustomobject][ordered]@{
                id = $matches[1]
                title = $matches[2].Trim()
            }
        }
    }
}

$reportIndexPath = Join-Path $resolvedPlaywrightReportDir "index.html"
$hasHtmlReport = Test-Path $reportIndexPath
$hasTestResultsDir = Test-Path $resolvedTestResultsDir
$traceCount = if ($hasTestResultsDir) {
    @(Get-ChildItem -Path $resolvedTestResultsDir -Filter trace.zip -Recurse -ErrorAction SilentlyContinue).Count
} else {
    0
}
$screenshotCount = if ($hasTestResultsDir) {
    @(Get-ChildItem -Path $resolvedTestResultsDir -Include *.png -Recurse -File -ErrorAction SilentlyContinue).Count
} else {
    0
}
$videoCount = if ($hasTestResultsDir) {
    @(Get-ChildItem -Path $resolvedTestResultsDir -Include *.webm,*.mp4 -Recurse -File -ErrorAction SilentlyContinue).Count
} else {
    0
}

$externalBlockers = New-Object System.Collections.Generic.List[string]
if ($remainingSprint12Tasks.id -contains "T12.6") {
    [void]$externalBlockers.Add("msix_real_certificate_required")
    [void]$externalBlockers.Add("trusted_msix_install_host_required")
}
if ($remainingSprint12Tasks.id -contains "T12.7") {
    [void]$externalBlockers.Add("release_publication_required")
}

$issueCodes = New-Object System.Collections.Generic.List[string]
if ([int]$planning.remaining_tasks -gt 0) {
    Add-IssueCode -List $issueCodes -Code "planning.remaining_tasks_present"
}
if ($releasePreflight.status -ne "ready") {
    foreach ($code in @($releasePreflight.issue_codes)) {
        Add-IssueCode -List $issueCodes -Code ("release.preflight.{0}" -f $code)
    }
}
if ($IncludeReleaseDryRun -and $null -ne $releaseDryRun -and $releaseDryRun.status -ne "ready") {
    foreach ($code in @($releaseDryRun.issue_codes)) {
        Add-IssueCode -List $issueCodes -Code ("release.dry_run.{0}" -f $code)
    }
}
if ($RequireEvidence -and -not $hasHtmlReport) {
    Add-IssueCode -List $issueCodes -Code "evidence.html_report_missing"
}
if ($RequireEvidence -and $traceCount -eq 0) {
    Add-IssueCode -List $issueCodes -Code "evidence.trace_missing"
}
if ($externalBlockers.Count -gt 0) {
    Add-IssueCode -List $issueCodes -Code "release.external_blockers_present"
}

$status = "ready"
if ($releasePreflight.status -ne "ready" -or ($RequireEvidence -and (-not $hasHtmlReport -or $traceCount -eq 0))) {
    $status = "blocked"
} elseif ([int]$planning.remaining_tasks -gt 0 -or $externalBlockers.Count -gt 0) {
    $status = "warning"
}

$payload = [pscustomobject][ordered]@{
    execution = "project-status"
    status = $status
    planning = [pscustomobject][ordered]@{
        sprint_count = [int]$planning.sprint_count
        total_tasks = [int]$planning.total_tasks
        done_tasks = [int]$planning.done_tasks
        remaining_tasks = [int]$planning.remaining_tasks
        sprints = @($planning.sprints)
    }
    sprint_12 = [pscustomobject][ordered]@{
        remaining_tasks = @($remainingSprint12Tasks)
    }
    release = [pscustomobject][ordered]@{
        preflight = $releasePreflight
        dry_run = $releaseDryRun
    }
    evidence = [pscustomobject][ordered]@{
        report_dir = $resolvedPlaywrightReportDir
        test_results_dir = $resolvedTestResultsDir
        has_html_report = $hasHtmlReport
        trace_count = $traceCount
        screenshot_count = $screenshotCount
        video_count = $videoCount
    }
    external_blockers = @($externalBlockers)
    issue_codes = @($issueCodes)
}

if ($Format -eq "json") {
    $output = $payload | ConvertTo-Json -Depth 8
} else {
    $lines = New-Object System.Collections.Generic.List[string]
    [void]$lines.Add("# Project Status")
    [void]$lines.Add("")
    [void]$lines.Add("## Planning")
    [void]$lines.Add("")
    [void]$lines.Add("- Sprints: $($payload.planning.sprint_count)")
    [void]$lines.Add("- Total tasks: $($payload.planning.total_tasks)")
    [void]$lines.Add("- Done tasks: $($payload.planning.done_tasks)")
    [void]$lines.Add("- Remaining tasks: $($payload.planning.remaining_tasks)")
    [void]$lines.Add("")
    [void]$lines.Add("## Release")
    [void]$lines.Add("")
    [void]$lines.Add("- Status: $($payload.release.preflight.status)")
    [void]$lines.Add("- Package version: $($payload.release.preflight.package_version)")
    [void]$lines.Add("- CMake version: $($payload.release.preflight.cmake_version)")
    [void]$lines.Add("- Has CLI binary: $($payload.release.preflight.has_cli_binary)")
    if ($IncludeReleaseDryRun -and $null -ne $payload.release.dry_run) {
        [void]$lines.Add("- Dry-run status: $($payload.release.dry_run.status)")
    }
    [void]$lines.Add("")
    [void]$lines.Add("## Evidence")
    [void]$lines.Add("")
    [void]$lines.Add("- HTML report: $($payload.evidence.has_html_report)")
    [void]$lines.Add("- Trace count: $($payload.evidence.trace_count)")
    [void]$lines.Add("- Screenshot count: $($payload.evidence.screenshot_count)")
    [void]$lines.Add("- Video count: $($payload.evidence.video_count)")
    [void]$lines.Add("")
    [void]$lines.Add("## Remaining Work")
    [void]$lines.Add("")
    foreach ($task in $payload.sprint_12.remaining_tasks) {
        [void]$lines.Add("- $($task.id): $($task.title)")
    }
    if ($payload.sprint_12.remaining_tasks.Count -eq 0) {
        [void]$lines.Add("- No open Sprint 12 tasks.")
    }
    if ($payload.external_blockers.Count -gt 0) {
        [void]$lines.Add("")
        [void]$lines.Add("## External Blockers")
        [void]$lines.Add("")
        foreach ($blocker in $payload.external_blockers) {
            [void]$lines.Add("- $blocker")
        }
    }
    $output = $lines -join [Environment]::NewLine
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    Write-Output $output
} else {
    $resolvedOutputPath = Resolve-LocalPath $OutputPath
    $directory = Split-Path -Parent $resolvedOutputPath
    if (-not [string]::IsNullOrWhiteSpace($directory) -and -not (Test-Path $directory)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($resolvedOutputPath, $output, $utf8NoBom)
    Write-Host "Project status written to $resolvedOutputPath"
}

if ($status -eq "blocked") {
    exit 1
}
