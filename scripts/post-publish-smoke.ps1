param(
    [string]$ArtifactPath,
    [string]$WorkingDir = "",
    [string]$DevCertificatePassword = "",
    [switch]$EnableDevMsixSmoke
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ArtifactPath)) {
    throw "ArtifactPath is required."
}

if (-not (Test-Path $ArtifactPath)) {
    throw "Artifact not found: $ArtifactPath"
}

$resolvedArtifact = (Resolve-Path $ArtifactPath).Path
$extension = [System.IO.Path]::GetExtension($resolvedArtifact).ToLowerInvariant()

if ([string]::IsNullOrWhiteSpace($WorkingDir)) {
    $WorkingDir = Join-Path ([System.IO.Path]::GetTempPath()) ("us4-post-publish-smoke-" + [System.Guid]::NewGuid().ToString("N"))
}

if (Test-Path $WorkingDir) {
    Remove-Item -Recurse -Force $WorkingDir
}
New-Item -ItemType Directory -Path $WorkingDir -Force | Out-Null

function Get-MsixIdentityName {
    param(
        [string]$PackagePath
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($PackagePath)
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

    return $identityName
}

switch ($extension) {
    ".zip" {
        Expand-Archive -Path $resolvedArtifact -DestinationPath $WorkingDir -Force
        $cliPath = Join-Path $WorkingDir "us4-cli.exe"
        if (-not (Test-Path $cliPath)) {
            throw "Portable artifact does not contain us4-cli.exe"
        }

        $versionOutput = & $cliPath version
        if ($LASTEXITCODE -ne 0) {
            throw "Smoke version command failed."
        }

        $probeOutput = & $cliPath probe --format json
        if ($LASTEXITCODE -ne 0) {
            throw "Smoke probe command failed."
        }

        $probeJson = $probeOutput | ConvertFrom-Json
        if ($probeJson.execution -ne "probe") {
            throw "Smoke probe JSON contract is invalid."
        }

        Write-Host "Portable zip smoke passed for $resolvedArtifact"
        break
    }
    ".msix" {
        if (-not $EnableDevMsixSmoke) {
            throw "MSIX smoke requires -EnableDevMsixSmoke for local self-signed validation or a dedicated signed install host for production artifacts."
        }

        if ([string]::IsNullOrWhiteSpace($DevCertificatePassword)) {
            throw "DevCertificatePassword is required when -EnableDevMsixSmoke is set."
        }

        $certificateThumbprint = ""
        $packageFullName = ""
        $signedArtifactPath = Join-Path $WorkingDir ([System.IO.Path]::GetFileName($resolvedArtifact))

        try {
            $createCertResult = & (Join-Path (Get-Location) "scripts\create-dev-signing-cert.ps1") `
                -OutputDir (Join-Path $WorkingDir "cert") `
                -CertificatePassword $DevCertificatePassword `
                -InstallTrustedRoot `
                -Format json | ConvertFrom-Json
            if ($createCertResult.status -ne "ready") {
                $issueCodes = @($createCertResult.issue_codes) -join ","
                throw "Local dev MSIX signing prerequisites are unavailable: $issueCodes"
            }
            $certificateThumbprint = $createCertResult.thumbprint

            Copy-Item -LiteralPath $resolvedArtifact -Destination $signedArtifactPath -Force

            & (Join-Path (Get-Location) "scripts\sign-msix.ps1") `
                -PackagePath $signedArtifactPath `
                -CertificatePath $createCertResult.pfx_path `
                -CertificatePassword $DevCertificatePassword `
                -TimestampUrl "" `
                -WorkingDir (Join-Path $WorkingDir "signing") *> $null

            & (Join-Path (Get-Location) "scripts\install-msix-smoke.ps1") `
                -PackagePath $signedArtifactPath *> $null

            $identityName = Get-MsixIdentityName -PackagePath $signedArtifactPath
            $installedPackage = Get-AppxPackage -Name $identityName -ErrorAction SilentlyContinue
            if ($installedPackage) {
                $packageFullName = $installedPackage.PackageFullName
            }

            Write-Host "Dev MSIX smoke passed for $resolvedArtifact"
        }
        finally {
            if (-not [string]::IsNullOrWhiteSpace($packageFullName)) {
                Remove-AppxPackage -Package $packageFullName -ErrorAction SilentlyContinue
            }

            if (-not [string]::IsNullOrWhiteSpace($certificateThumbprint)) {
                & (Join-Path (Get-Location) "scripts\remove-dev-signing-cert.ps1") `
                    -Thumbprint $certificateThumbprint *> $null
            }
        }
        break
    }
    default {
        throw "Unsupported artifact type: $extension"
    }
}
