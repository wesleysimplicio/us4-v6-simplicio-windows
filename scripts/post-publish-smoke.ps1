param(
    [string]$ArtifactPath,
    [string]$WorkingDir = ""
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
        throw "MSIX smoke is not automated locally yet. Use a signed MSIX and a dedicated install host for post-publish validation."
    }
    default {
        throw "Unsupported artifact type: $extension"
    }
}
