param(
    [string]$SprintsDir = ".specs\\sprints",
    [string]$Format = "json",
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

if ($Format -notin @("json", "markdown")) {
    throw "Unknown value for -Format. Use json or markdown."
}

$resolvedSprintsDir = if ([System.IO.Path]::IsPathRooted($SprintsDir)) {
    $SprintsDir
} else {
    Join-Path (Get-Location) $SprintsDir
}

if (-not (Test-Path $resolvedSprintsDir)) {
    throw "Sprints directory not found: $resolvedSprintsDir"
}

$sprintDirs = Get-ChildItem $resolvedSprintsDir -Directory | Where-Object { $_.Name -like "sprint-*" } | Sort-Object Name
$sprintEntries = New-Object System.Collections.ArrayList
$totalTasks = 0
$doneTasks = 0

foreach ($dir in $sprintDirs) {
    $sprintFile = Join-Path $dir.FullName "SPRINT.md"
    if (-not (Test-Path $sprintFile)) {
        continue
    }

    $content = Get-Content $sprintFile
    $header = $content | Select-Object -First 12
    $statusLine = $header | Where-Object { $_ -match '^status:' } | Select-Object -First 1
    $status = if ($statusLine) { ($statusLine -replace '^status:\s*', '').Trim() } else { "unknown" }

    $taskLines = $content | Where-Object { $_ -match '^- \[( |x)\] T' }
    $sprintTotal = $taskLines.Count
    $sprintDone = ($taskLines | Where-Object { $_ -match '^- \[x\] T' }).Count
    $sprintRemaining = $sprintTotal - $sprintDone

    $totalTasks += $sprintTotal
    $doneTasks += $sprintDone

    [void]$sprintEntries.Add([pscustomobject][ordered]@{
        sprint = $dir.Name
        status = $status
        total_tasks = $sprintTotal
        done_tasks = $sprintDone
        remaining_tasks = $sprintRemaining
    })
}

$payload = [pscustomobject][ordered]@{
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    sprint_count = $sprintEntries.Count
    total_tasks = $totalTasks
    done_tasks = $doneTasks
    remaining_tasks = ($totalTasks - $doneTasks)
    sprints = @($sprintEntries.ToArray())
}

if ($Format -eq "json") {
    $output = $payload | ConvertTo-Json -Depth 6
} else {
    $lines = New-Object System.Collections.Generic.List[string]
    [void]$lines.Add("# Planning Status")
    [void]$lines.Add("")
    [void]$lines.Add('> Generated from `sprint-XX/SPRINT.md` frontmatter and versioned task checkboxes.')
    [void]$lines.Add("")
    [void]$lines.Add("- Generated: $($payload.generated_at_utc)")
    [void]$lines.Add("- Sprints: $($payload.sprint_count)")
    [void]$lines.Add("- Total tasks: $($payload.total_tasks)")
    [void]$lines.Add("- Done tasks: $($payload.done_tasks)")
    [void]$lines.Add("- Remaining tasks: $($payload.remaining_tasks)")
    [void]$lines.Add("")
    [void]$lines.Add("| Sprint | Status | Done | Remaining | Total |")
    [void]$lines.Add("|---|---|---:|---:|---:|")
    foreach ($entry in $payload.sprints) {
        [void]$lines.Add("| $($entry.sprint) | $($entry.status) | $($entry.done_tasks) | $($entry.remaining_tasks) | $($entry.total_tasks) |")
    }
    $output = $lines -join [Environment]::NewLine
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    Write-Output $output
} else {
    $resolvedOutputPath = if ([System.IO.Path]::IsPathRooted($OutputPath)) {
        $OutputPath
    } else {
        Join-Path (Get-Location) $OutputPath
    }
    $directory = Split-Path -Parent $resolvedOutputPath
    if (-not [string]::IsNullOrWhiteSpace($directory) -and -not (Test-Path $directory)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($resolvedOutputPath, $output, $utf8NoBom)
    Write-Host "Planning status written to $resolvedOutputPath"
}
