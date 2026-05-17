param(
    [string]$BuildDir = "build",
    [string]$OutputDir = "dist",
    [string]$WorkingDir = "",
    [string]$CertificatePassword = "",
    [string]$Format = "text",
    [switch]$PreflightOnly
)

$ErrorActionPreference = "Stop"

if ($Format -notin @("text", "json")) {
    throw "Unknown value for -Format. Use text or json."
}

if (-not $PreflightOnly -and [string]::IsNullOrWhiteSpace($CertificatePassword)) {
    throw "CertificatePassword is required."
}

$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir
} else {
    Join-Path (Get-Location) $BuildDir
}

$resolvedOutputDir = if ([System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir
} else {
    Join-Path (Get-Location) $OutputDir
}

if ([string]::IsNullOrWhiteSpace($WorkingDir)) {
    $WorkingDir = Join-Path ([System.IO.Path]::GetTempPath()) ("us4-dev-msix-smoke-" + [System.Guid]::NewGuid().ToString("N"))
} elseif (-not [System.IO.Path]::IsPathRooted($WorkingDir)) {
    $WorkingDir = Join-Path (Get-Location) $WorkingDir
}

$requiredCommands = @{
    makeappx = "MakeAppx.exe"
    signtool = "signtool.exe"
    new_self_signed_certificate = "New-SelfSignedCertificate"
    export_pfx_certificate = "Export-PfxCertificate"
    export_certificate = "Export-Certificate"
    import_certificate = "Import-Certificate"
    add_appx_package = "Add-AppxPackage"
    get_appx_package = "Get-AppxPackage"
}

$issues = New-Object System.Collections.Generic.List[string]
foreach ($entry in $requiredCommands.GetEnumerator()) {
    if (-not (Get-Command $entry.Value -ErrorAction SilentlyContinue)) {
        [void]$issues.Add(("{0}_missing" -f $entry.Key))
    }
}

$steps = New-Object System.Collections.Generic.List[object]
function Add-Step {
    param(
        [string]$Name,
        [string]$Status,
        [string]$Detail = ""
    )

    [void]$steps.Add([pscustomobject][ordered]@{
        name = $Name
        status = $Status
        detail = $Detail
    })
}

if ($issues.Count -gt 0) {
    $payload = [ordered]@{
        execution = "dev-msix-smoke"
        status = "blocked"
        build_dir = $resolvedBuildDir
        output_dir = $resolvedOutputDir
        working_dir = $WorkingDir
        steps = @($steps)
        issue_codes = @($issues)
    }

    if ($Format -eq "json") {
        $payload | ConvertTo-Json -Depth 6
    } else {
        Write-Host "execution: dev-msix-smoke"
        Write-Host "status: blocked"
        Write-Host ("issue_codes: " + ($issues -join ","))
    }
    exit 1
}

if ($PreflightOnly) {
    $payload = [ordered]@{
        execution = "dev-msix-smoke"
        status = "ready"
        build_dir = $resolvedBuildDir
        output_dir = $resolvedOutputDir
        working_dir = $WorkingDir
        preflight_only = $true
        steps = @(
            [pscustomobject][ordered]@{
                name = "preflight"
                status = "passed"
                detail = ""
            }
        )
        issue_codes = @()
    }

    if ($Format -eq "json") {
        $payload | ConvertTo-Json -Depth 6
    } else {
        Write-Host "execution: dev-msix-smoke"
        Write-Host "status: ready"
        Write-Host "preflight_only: true"
        Write-Host "issue_codes: none"
    }
    exit 0
}

New-Item -ItemType Directory -Path $resolvedOutputDir -Force | Out-Null
New-Item -ItemType Directory -Path $WorkingDir -Force | Out-Null

$certificateThumbprint = ""
$packageFullName = ""
$msixPath = ""

try {
    $createCertResult = & (Join-Path (Get-Location) "scripts\create-dev-signing-cert.ps1") `
        -OutputDir (Join-Path $WorkingDir "cert") `
        -CertificatePassword $CertificatePassword `
        -InstallTrustedRoot `
        -Format json | ConvertFrom-Json
    $certificateThumbprint = $createCertResult.thumbprint
    Add-Step -Name "create-dev-signing-cert" -Status "passed"

    & (Join-Path (Get-Location) "scripts\build-msix.ps1") `
        -BuildDir $resolvedBuildDir `
        -OutputDir $resolvedOutputDir `
        -WorkingDir (Join-Path $WorkingDir "msix-package") *> $null
    $msixPath = Get-ChildItem -Path $resolvedOutputDir -Filter *.msix | Select-Object -First 1 -ExpandProperty FullName
    if ([string]::IsNullOrWhiteSpace($msixPath)) {
        throw "MSIX artifact not found after build."
    }
    Add-Step -Name "build-msix" -Status "passed"

    & (Join-Path (Get-Location) "scripts\sign-msix.ps1") `
        -PackagePath $msixPath `
        -CertificatePath $createCertResult.pfx_path `
        -CertificatePassword $CertificatePassword `
        -WorkingDir (Join-Path $WorkingDir "signing") *> $null
    Add-Step -Name "sign-msix" -Status "passed"

    & (Join-Path (Get-Location) "scripts\install-msix-smoke.ps1") `
        -PackagePath $msixPath *> $null
    Add-Step -Name "install-msix-smoke" -Status "passed"

    $installedPackage = Get-AppxPackage -Name "WesleySimplicio.US4V6WindowsEdition" -ErrorAction SilentlyContinue
    if ($installedPackage) {
        $packageFullName = $installedPackage.PackageFullName
    }
}
catch {
    [void]$issues.Add($_.Exception.Message)
    Add-Step -Name "dev-msix-smoke" -Status "failed" -Detail $_.Exception.Message
}
finally {
    if (-not [string]::IsNullOrWhiteSpace($packageFullName)) {
        try {
            Remove-AppxPackage -Package $packageFullName -ErrorAction Stop
            Add-Step -Name "remove-appx-package" -Status "passed"
        } catch {
            Add-Step -Name "remove-appx-package" -Status "failed" -Detail $_.Exception.Message
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($certificateThumbprint)) {
        try {
            & (Join-Path (Get-Location) "scripts\remove-dev-signing-cert.ps1") `
                -Thumbprint $certificateThumbprint *> $null
            Add-Step -Name "remove-dev-signing-cert" -Status "passed"
        } catch {
            Add-Step -Name "remove-dev-signing-cert" -Status "failed" -Detail $_.Exception.Message
        }
    }
}

$status = if ($issues.Count -eq 0) { "ready" } else { "blocked" }
$payload = [ordered]@{
    execution = "dev-msix-smoke"
    status = $status
    build_dir = $resolvedBuildDir
    output_dir = $resolvedOutputDir
    working_dir = $WorkingDir
    msix_path = $msixPath
    certificate_thumbprint = $certificateThumbprint
    steps = @($steps)
    issue_codes = @($issues)
}

if ($Format -eq "json") {
    $payload | ConvertTo-Json -Depth 6
} else {
    Write-Host "execution: dev-msix-smoke"
    Write-Host "status: $status"
    Write-Host "output_dir: $resolvedOutputDir"
    if (-not [string]::IsNullOrWhiteSpace($msixPath)) {
        Write-Host "msix_path: $msixPath"
    }
    if (-not [string]::IsNullOrWhiteSpace($certificateThumbprint)) {
        Write-Host "certificate_thumbprint: $certificateThumbprint"
    }
    foreach ($step in $steps) {
        if ([string]::IsNullOrWhiteSpace($step.detail)) {
            Write-Host ("step: {0} => {1}" -f $step.name, $step.status)
        } else {
            Write-Host ("step: {0} => {1} ({2})" -f $step.name, $step.status, $step.detail)
        }
    }
    if ($issues.Count -gt 0) {
        Write-Host ("issue_codes: " + ($issues -join ","))
    } else {
        Write-Host "issue_codes: none"
    }
}

if ($status -ne "ready") {
    exit 1
}
