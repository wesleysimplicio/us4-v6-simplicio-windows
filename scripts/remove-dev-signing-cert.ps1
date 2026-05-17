param(
    [string]$Thumbprint,
    [string]$Format = "text"
)

$ErrorActionPreference = "Stop"

if ($Format -notin @("text", "json")) {
    throw "Unknown value for -Format. Use text or json."
}

if ([string]::IsNullOrWhiteSpace($Thumbprint)) {
    throw "Thumbprint is required."
}

$stores = @(
    "Cert:\CurrentUser\My",
    "Cert:\CurrentUser\Root",
    "Cert:\CurrentUser\TrustedPeople"
)

$removedStores = New-Object System.Collections.Generic.List[string]
foreach ($storePath in $stores) {
    $certificatePath = Join-Path $storePath $Thumbprint
    if (Test-Path $certificatePath) {
        Remove-Item -LiteralPath $certificatePath -Force
        [void]$removedStores.Add($storePath.Replace("Cert:\", "").Replace("\", "\\"))
    }
}

$payload = [ordered]@{
    execution = "remove-dev-signing-cert"
    status = "ready"
    thumbprint = $Thumbprint
    removed_stores = @($removedStores)
    issue_codes = @()
}

if ($Format -eq "json") {
    $payload | ConvertTo-Json -Depth 5
} else {
    Write-Host "execution: remove-dev-signing-cert"
    Write-Host "status: ready"
    Write-Host "thumbprint: $Thumbprint"
    if ($removedStores.Count -gt 0) {
        Write-Host ("removed_stores: " + ($removedStores -join ","))
    } else {
        Write-Host "removed_stores: none"
    }
}
