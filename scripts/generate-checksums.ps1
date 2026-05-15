param(
    [string]$OutputDir = "dist"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $OutputDir)) {
    throw "Output directory not found: $OutputDir"
}

$artifacts = Get-ChildItem -Path $OutputDir -File | Where-Object {
    $_.Extension -in @(".zip", ".msix", ".txt")
} | Where-Object {
    $_.Name -ne "SHA256SUMS.txt"
}

if ($artifacts.Count -eq 0) {
    throw "No release artifacts found under $OutputDir"
}

$checksumsPath = Join-Path $OutputDir "SHA256SUMS.txt"
$lines = foreach ($artifact in $artifacts | Sort-Object Name) {
    $hash = (Get-FileHash -Algorithm SHA256 -Path $artifact.FullName).Hash.ToLowerInvariant()
    "$hash  $($artifact.Name)"
}

Set-Content -Path $checksumsPath -Value $lines -Encoding ASCII
Write-Host "Checksums written to $checksumsPath"
