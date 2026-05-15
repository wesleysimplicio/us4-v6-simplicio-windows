param(
    [string]$PackagePath,
    [string]$CertificatePath = "",
    [string]$CertificatePassword = "",
    [string]$TimestampUrl = "http://timestamp.digicert.com",
    [string]$WorkingDir = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($PackagePath)) {
    throw "PackagePath is required."
}

if (-not (Test-Path $PackagePath)) {
    throw "Package not found: $PackagePath"
}

if ([string]::IsNullOrWhiteSpace($CertificatePath)) {
    $CertificatePath = $env:US4_SIGN_CERT_PATH
}

if ([string]::IsNullOrWhiteSpace($CertificatePassword)) {
    $CertificatePassword = $env:US4_SIGN_CERT_PASSWORD
}

$temporaryCertificatePath = $null
$base64Certificate = $env:US4_SIGN_CERT_BASE64

if ([string]::IsNullOrWhiteSpace($CertificatePath) -and -not [string]::IsNullOrWhiteSpace($base64Certificate)) {
    if ([string]::IsNullOrWhiteSpace($WorkingDir)) {
        $WorkingDir = Join-Path ([System.IO.Path]::GetTempPath()) ("us4-sign-msix-" + [System.Guid]::NewGuid().ToString("N"))
    }
    New-Item -ItemType Directory -Path $WorkingDir -Force | Out-Null
    $temporaryCertificatePath = Join-Path $WorkingDir "us4-signing-cert.pfx"
    [System.IO.File]::WriteAllBytes($temporaryCertificatePath, [Convert]::FromBase64String($base64Certificate))
    $CertificatePath = $temporaryCertificatePath
}

if ([string]::IsNullOrWhiteSpace($CertificatePath) -or [string]::IsNullOrWhiteSpace($CertificatePassword)) {
    throw "MSIX signing requires certificate configuration. Set US4_SIGN_CERT_PATH and US4_SIGN_CERT_PASSWORD, or US4_SIGN_CERT_BASE64 and US4_SIGN_CERT_PASSWORD."
}

if (-not (Test-Path $CertificatePath)) {
    throw "Signing certificate not found: $CertificatePath"
}

$signtool = Get-Command signtool.exe -ErrorAction SilentlyContinue
if (-not $signtool) {
    throw "signtool.exe not found. Install Windows SDK signing tools before signing MSIX artifacts."
}

& $signtool.Source sign /fd SHA256 /td SHA256 /tr $TimestampUrl /f $CertificatePath /p $CertificatePassword $PackagePath
if ($LASTEXITCODE -ne 0) {
    throw "signtool.exe failed while signing $PackagePath"
}

Write-Host "Signed MSIX package at $PackagePath"
