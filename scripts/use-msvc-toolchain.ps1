$ErrorActionPreference = "Stop"

function Get-HighestVersionDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    if (-not (Test-Path $Root)) {
        return $null
    }

    $directories = Get-ChildItem -Path $Root -Directory |
        Sort-Object { [version]$_.Name } -Descending
    return $directories | Select-Object -First 1
}

function Get-VsWherePath {
    $installerRoot = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\\Installer"
    $vsWherePath = Join-Path $installerRoot "vswhere.exe"
    if (Test-Path $vsWherePath) {
        return $vsWherePath
    }

    return $null
}

function Get-VisualStudioInstallationPath {
    $vsWherePath = Get-VsWherePath
    if ($vsWherePath) {
        $installPath = & $vsWherePath -latest -products * `
            -requires Microsoft.VisualStudio.Workload.NativeDesktop `
            -property installationPath
        if ($installPath) {
            return $installPath.Trim()
        }
    }

    $roots = @(
        "C:\\Program Files\\Microsoft Visual Studio\\18"
        "C:\\Program Files\\Microsoft Visual Studio\\17"
    )
    $candidates = foreach ($root in $roots) {
        if (-not (Test-Path $root)) {
            continue
        }

        Get-ChildItem -Path $root -Directory | ForEach-Object {
            $installPath = $_.FullName
            $msvcRoot = Join-Path $installPath "VC\\Tools\\MSVC"
            if (Test-Path $msvcRoot) {
                [pscustomobject]@{
                    Path = $installPath
                    Name = $_.Name
                }
            }
        }
    }

    return ($candidates | Sort-Object Name -Descending | Select-Object -First 1).Path
}

function Get-ToolchainSnapshot {
    $visualStudioPath = Get-VisualStudioInstallationPath
    if (-not $visualStudioPath) {
        return $null
    }

    $msvcRoot = Join-Path $visualStudioPath "VC\\Tools\\MSVC"
    $msvcVersionDirectory = Get-HighestVersionDirectory -Root $msvcRoot
    if (-not $msvcVersionDirectory) {
        return $null
    }

    $sdkRoot = "C:\\Program Files (x86)\\Windows Kits\\10"
    $sdkIncludeDirectory = Get-HighestVersionDirectory -Root (Join-Path $sdkRoot "Include")
    if (-not $sdkIncludeDirectory) {
        return $null
    }

    $snapshot = [ordered]@{
        VisualStudioPath  = $visualStudioPath
        MsvcToolsPath     = $msvcVersionDirectory.FullName
        WindowsSdkRoot    = $sdkRoot
        WindowsSdkVersion = $sdkIncludeDirectory.Name
        NetFxSdkRoot      = "C:\\Program Files (x86)\\Windows Kits\\NETFXSDK\\4.8"
        CMakePath         = Join-Path $visualStudioPath "Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin"
        NinjaPath         = Join-Path $visualStudioPath "Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\Ninja"
    }

    return [pscustomobject]$snapshot
}

function Enable-MsvcToolchain {
    if ((Get-Command cl -ErrorAction SilentlyContinue) -and
        (Get-Command cmake -ErrorAction SilentlyContinue) -and
        (Get-Command ninja -ErrorAction SilentlyContinue)) {
        return $true
    }

    $snapshot = Get-ToolchainSnapshot
    if (-not $snapshot) {
        return $false
    }

    $msvcBin = Join-Path $snapshot.MsvcToolsPath "bin\\Hostx64\\x64"
    $sdkBin = Join-Path $snapshot.WindowsSdkRoot ("bin\\" + $snapshot.WindowsSdkVersion + "\\x64")
    $msvcInclude = Join-Path $snapshot.MsvcToolsPath "include"
    $vsInclude = Join-Path $snapshot.VisualStudioPath "VC\\Auxiliary\\VS\\include"
    $sdkIncludeRoot = Join-Path $snapshot.WindowsSdkRoot ("include\\" + $snapshot.WindowsSdkVersion)
    $msvcLib = Join-Path $snapshot.MsvcToolsPath "lib\\x64"
    $sdkLibRoot = Join-Path $snapshot.WindowsSdkRoot ("lib\\" + $snapshot.WindowsSdkVersion)
    $netFxLib = Join-Path $snapshot.NetFxSdkRoot "lib\\um\\x64"
    $pathsToPrepend = @(
        $msvcBin
        $snapshot.NinjaPath
        $snapshot.CMakePath
        $sdkBin
    )

    $vcpkgRoot = Join-Path $env:USERPROFILE "vcpkg"
    $vcpkgInstalledRoot = Join-Path $vcpkgRoot "installed\\x64-windows"
    $vcpkgBin = Join-Path $vcpkgInstalledRoot "bin"
    if (Test-Path $vcpkgBin) {
        $pathsToPrepend += $vcpkgBin
    }

    $env:PATH = (($pathsToPrepend + @($env:PATH)) -join ";")
    $env:INCLUDE = @(
        $msvcInclude
        $vsInclude
        (Join-Path $sdkIncludeRoot "ucrt")
        (Join-Path $sdkIncludeRoot "um")
        (Join-Path $sdkIncludeRoot "shared")
        (Join-Path $sdkIncludeRoot "winrt")
        (Join-Path $sdkIncludeRoot "cppwinrt")
        (Join-Path $snapshot.NetFxSdkRoot "include\\um")
    ) -join ";"
    $env:LIB = @(
        $msvcLib
        $netFxLib
        (Join-Path $sdkLibRoot "ucrt\\x64")
        (Join-Path $sdkLibRoot "um\\x64")
    ) -join ";"
    $env:LIBPATH = @(
        $msvcLib
        (Join-Path $snapshot.MsvcToolsPath "lib\\x86\\store\\references")
        "C:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319"
    ) -join ";"

    if (Test-Path $vcpkgInstalledRoot) {
        if ($env:CMAKE_PREFIX_PATH) {
            $env:CMAKE_PREFIX_PATH = $vcpkgInstalledRoot + ";" + $env:CMAKE_PREFIX_PATH
        } else {
            $env:CMAKE_PREFIX_PATH = $vcpkgInstalledRoot
        }
    }

    return (Get-Command cl -ErrorAction SilentlyContinue) -and
           (Get-Command cmake -ErrorAction SilentlyContinue) -and
           (Get-Command ninja -ErrorAction SilentlyContinue)
}
