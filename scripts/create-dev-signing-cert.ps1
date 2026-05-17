param(
    [string]$OutputDir = "",
    [string]$Subject = "CN=US4 Dev",
    [string]$CertificatePassword = "",
    [int]$ValidDays = 30,
    [string]$Format = "text",
    [switch]$InstallTrustedRoot
)

$ErrorActionPreference = "Stop"

if ($Format -notin @("text", "json")) {
    throw "Unknown value for -Format. Use text or json."
}

if ([string]::IsNullOrWhiteSpace($CertificatePassword)) {
    throw "CertificatePassword is required."
}

if ($ValidDays -lt 1) {
    throw "ValidDays must be greater than zero."
}

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path ([System.IO.Path]::GetTempPath()) ("us4-dev-cert-" + [System.Guid]::NewGuid().ToString("N"))
}

$resolvedOutputDir = if ([System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir
} else {
    Join-Path (Get-Location) $OutputDir
}

$issues = New-Object System.Collections.Generic.List[string]
$requiredCommands = @(
    "New-SelfSignedCertificate",
    "Export-PfxCertificate",
    "Export-Certificate"
)

if ($InstallTrustedRoot) {
    $requiredCommands += "Import-Certificate"
}

foreach ($commandName in $requiredCommands) {
    if (-not (Get-Command $commandName -ErrorAction SilentlyContinue)) {
        [void]$issues.Add(("{0}_missing" -f $commandName.ToLowerInvariant()))
    }
}

if ($issues.Count -gt 0) {
    $payload = [ordered]@{
        execution = "create-dev-signing-cert"
        status = "blocked"
        output_dir = $resolvedOutputDir
        subject = $Subject
        install_trusted_root = [bool]$InstallTrustedRoot
        issue_codes = @($issues)
    }

    if ($Format -eq "json") {
        $payload | ConvertTo-Json -Depth 5
    } else {
        Write-Host "execution: create-dev-signing-cert"
        Write-Host "status: blocked"
        Write-Host "output_dir: $resolvedOutputDir"
        Write-Host "subject: $Subject"
        Write-Host "install_trusted_root: $($InstallTrustedRoot.ToString().ToLowerInvariant())"
        Write-Host ("issue_codes: " + ($issues -join ","))
    }
    exit 1
}

New-Item -ItemType Directory -Path $resolvedOutputDir -Force | Out-Null

$securePassword = ConvertTo-SecureString $CertificatePassword -AsPlainText -Force
$certificate = New-SelfSignedCertificate `
    -Type CodeSigningCert `
    -Subject $Subject `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -KeyExportPolicy Exportable `
    -NotAfter (Get-Date).AddDays($ValidDays) `
    -FriendlyName "US4 Dev Signing"

$certificatePath = Join-Path "Cert:\CurrentUser\My" $certificate.Thumbprint
$pfxPath = Join-Path $resolvedOutputDir "us4-dev-signing-cert.pfx"
$cerPath = Join-Path $resolvedOutputDir "us4-dev-signing-cert.cer"

Export-PfxCertificate -Cert $certificatePath -FilePath $pfxPath -Password $securePassword | Out-Null
Export-Certificate -Cert $certificatePath -FilePath $cerPath | Out-Null

$trustedStores = New-Object System.Collections.Generic.List[string]
if ($InstallTrustedRoot) {
    Import-Certificate -FilePath $cerPath -CertStoreLocation "Cert:\CurrentUser\Root" | Out-Null
    [void]$trustedStores.Add("CurrentUser\\Root")
    Import-Certificate -FilePath $cerPath -CertStoreLocation "Cert:\CurrentUser\TrustedPeople" | Out-Null
    [void]$trustedStores.Add("CurrentUser\\TrustedPeople")
}

$payload = [ordered]@{
    execution = "create-dev-signing-cert"
    status = "ready"
    output_dir = $resolvedOutputDir
    subject = $Subject
    thumbprint = $certificate.Thumbprint
    pfx_path = $pfxPath
    cer_path = $cerPath
    install_trusted_root = [bool]$InstallTrustedRoot
    trusted_stores = @($trustedStores)
    issue_codes = @()
}

if ($Format -eq "json") {
    $payload | ConvertTo-Json -Depth 5
} else {
    Write-Host "execution: create-dev-signing-cert"
    Write-Host "status: ready"
    Write-Host "output_dir: $resolvedOutputDir"
    Write-Host "subject: $Subject"
    Write-Host "thumbprint: $($certificate.Thumbprint)"
    Write-Host "pfx_path: $pfxPath"
    Write-Host "cer_path: $cerPath"
    Write-Host "install_trusted_root: $($InstallTrustedRoot.ToString().ToLowerInvariant())"
}
