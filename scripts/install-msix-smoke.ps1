param(
    [string]$PackagePath
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($PackagePath)) {
    throw "PackagePath is required."
}

if (-not (Test-Path $PackagePath)) {
    throw "Package not found: $PackagePath"
}

if (-not (Get-Command Add-AppxPackage -ErrorAction SilentlyContinue)) {
    throw "Add-AppxPackage is not available on this host."
}

if (-not (Get-Command Get-AppxPackage -ErrorAction SilentlyContinue)) {
    throw "Get-AppxPackage is not available on this host."
}

$signature = Get-AuthenticodeSignature -FilePath $PackagePath
if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid) {
    throw "MSIX package is not signed with a trusted certificate."
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path $PackagePath).Path)
try {
    $manifestEntry = $archive.Entries | Where-Object { $_.FullName -eq "AppxManifest.xml" } | Select-Object -First 1
    if (-not $manifestEntry) {
        throw "AppxManifest.xml not found in package."
    }

    $stream = $manifestEntry.Open()
    $reader = New-Object System.IO.StreamReader($stream)
    try {
        [xml]$manifest = $reader.ReadToEnd()
    }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}
finally {
    $archive.Dispose()
}

$identityName = $manifest.Package.Identity.Name
if ([string]::IsNullOrWhiteSpace($identityName)) {
    throw "Could not resolve MSIX identity name from manifest."
}

Add-AppxPackage -Path $PackagePath -ForceApplicationShutdown -ErrorAction Stop
$installedPackage = Get-AppxPackage -Name $identityName -ErrorAction SilentlyContinue
if (-not $installedPackage) {
    throw "MSIX install completed without a discoverable package entry for $identityName"
}

Write-Host "MSIX install smoke passed for $identityName"
