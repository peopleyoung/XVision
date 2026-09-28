function Get-XVisionFullFeatureArguments {
    param([string]$OpenCvRoot, [string]$OnnxRuntimeRoot)
    foreach ($Sdk in @(
        @{ Name = 'OpenCvRoot'; Path = $OpenCvRoot },
        @{ Name = 'OnnxRuntimeRoot'; Path = $OnnxRuntimeRoot }
    )) {
        if ([string]::IsNullOrWhiteSpace($Sdk.Path) -or
            -not (Test-Path -LiteralPath $Sdk.Path -PathType Container)) {
            throw "Set -$($Sdk.Name) to an existing SDK directory for a full-feature build."
        }
    }
    @(
        '-DBUILD_TESTING=ON',
        '-DXVISION_BUILD_COMMON_USING=ON',
        '-DXVISION_BUILD_SYSTEM_PLUGIN=ON',
        '-DXVISION_ENABLE_BREAKPAD=ON',
        '-DXVISION_ENABLE_OPENCV=ON',
        '-DXVISION_ENABLE_ONNXRUNTIME=ON',
        "-DXVISION_OPENCV_ROOT=$((Resolve-Path -LiteralPath $OpenCvRoot).ProviderPath)",
        "-DXVISION_ONNXRUNTIME_ROOT=$((Resolve-Path -LiteralPath $OnnxRuntimeRoot).ProviderPath)"
    )
}

function Assert-XVisionFullFeatureCache {
    param([string]$BuildDirectory)
    $CachePath = Join-Path $BuildDirectory 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $CachePath -PathType Leaf)) {
        throw "Missing CMake cache: $CachePath. Build the full-feature configuration first."
    }
    $Cache = @{}
    foreach ($Line in Get-Content -LiteralPath $CachePath) {
        if ($Line -match '^([^#/:][^:]*):[^=]+=(.*)$') {
            $Cache[$Matches[1]] = $Matches[2]
        }
    }
    foreach ($Option in @(
        'BUILD_TESTING', 'XVISION_BUILD_COMMON_USING',
        'XVISION_BUILD_SYSTEM_PLUGIN', 'XVISION_ENABLE_BREAKPAD',
        'XVISION_ENABLE_OPENCV', 'XVISION_ENABLE_ONNXRUNTIME'
    )) {
        if ($Cache[$Option] -notmatch '^(?i:ON|TRUE|YES|1)$') {
            throw "Full-feature packaging requires ${Option}=ON in $CachePath. Rebuild without -SkipBuild."
        }
    }
    foreach ($Option in @(
        'XVISION_OPENCV_RUNTIME_DLL', 'XVISION_ONNXRUNTIME_RUNTIME_DLL'
    )) {
        $Runtime = $Cache[$Option]
        if ([string]::IsNullOrWhiteSpace($Runtime) -or
            [System.IO.Path]::GetExtension($Runtime) -ine '.dll' -or
            -not (Test-Path -LiteralPath $Runtime -PathType Leaf)) {
            throw "Missing SDK runtime from ${Option}: $Runtime. Reconfigure with complete Windows SDKs."
        }
    }
    return $Cache
}

function Get-XVisionQtDeploymentArguments {
    param([string]$Configuration, [string]$PackageDirectory)
    @(
        "--$($Configuration.ToLowerInvariant())",
        '--no-translations', '--compiler-runtime', '--sql',
        '--dir', $PackageDirectory,
        (Join-Path $PackageDirectory 'XVision.exe')
    )
    foreach ($RelativePath in @(Get-XVisionQtRuntimeFiles $Configuration)) {
        if ([System.IO.Path]::GetFileName($RelativePath) -eq $RelativePath) {
            Join-Path $PackageDirectory $RelativePath
        }
    }
    Join-Path $PackageDirectory 'XvFuncCollection/XvFuncSystem.dll'
}

function Get-XVisionQtRuntimeFiles {
    param([ValidateSet('Debug', 'Release')][string]$Configuration)
    $Suffix = if ($Configuration -eq 'Debug') { 'd' } else { '' }
    foreach ($Module in @('Core', 'Gui', 'Widgets', 'Xml', 'Concurrent',
        'Network', 'SerialPort', 'SerialBus', 'Sql', 'StateMachine')) {
        "Qt6${Module}${Suffix}.dll"
    }
    "platforms/qwindows${Suffix}.dll"
    "sqldrivers/qsqlite${Suffix}.dll"
}


function Get-XVisionMsvcRuntimeFiles {
    param(
        [ValidateSet('Debug', 'Release')][string]$Configuration,
        [string]$VcInstallDirectory = $env:VCINSTALLDIR
    )
    if ([string]::IsNullOrWhiteSpace($VcInstallDirectory)) {
        throw 'Set VCINSTALLDIR to the Visual Studio VC directory to deploy the MSVC runtime.'
    }
    $RedistRoot = Join-Path $VcInstallDirectory 'Redist/MSVC'
    if (-not (Test-Path -LiteralPath $RedistRoot -PathType Container)) {
        throw "Missing MSVC redist directory: $RedistRoot"
    }
    $Versions = @(Get-ChildItem -LiteralPath $RedistRoot -Directory |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+(\.\d+)?$' } |
        Sort-Object { [version]$_.Name } -Descending)
    $Suffix = if ($Configuration -eq 'Debug') { 'd' } else { '' }
    $ArchitecturePath = if ($Configuration -eq 'Debug') { 'Debug_NonRedist/x64' } else { 'x64' }
    $CrtPattern = if ($Configuration -eq 'Debug') { 'Microsoft.VC*.DebugCRT' } else { 'Microsoft.VC*.CRT' }
    foreach ($Version in $Versions) {
        $ArchitectureDirectory = Join-Path $Version.FullName $ArchitecturePath
        if (-not (Test-Path -LiteralPath $ArchitectureDirectory -PathType Container)) { continue }
        $CrtDirectories = @(Get-ChildItem -LiteralPath $ArchitectureDirectory -Directory -Filter $CrtPattern)
        if ($CrtDirectories.Count -eq 0) { continue }
        $CrtDirectory = $CrtDirectories[0].FullName
        foreach ($RequiredName in @("msvcp140$Suffix.dll", "vcruntime140$Suffix.dll", "vcruntime140_1$Suffix.dll")) {
            if (-not (Test-Path -LiteralPath (Join-Path $CrtDirectory $RequiredName) -PathType Leaf)) {
                throw "Missing MSVC runtime $RequiredName in $CrtDirectory"
            }
        }
        return @(Get-ChildItem -LiteralPath $CrtDirectory -File -Filter '*.dll')
    }
    throw "No x64 $Configuration MSVC CRT directory found under $RedistRoot"
}
