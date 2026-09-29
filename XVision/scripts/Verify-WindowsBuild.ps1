param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$FullFeatures,
    [string]$OpenCvRoot = $env:XVISION_OPENCV_ROOT,
    [string]$OnnxRuntimeRoot = $env:XVISION_ONNXRUNTIME_ROOT,
    [switch]$SkipBuild,
    [ValidateRange(0, 1440)]
    [int]$SoakMinutes = 0
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot 'WindowsBuild.Common.ps1')
$SourceDir = Split-Path -Parent $PSScriptRoot
$Preset = if ($Configuration -eq "Debug") {
    "windows-msvc2019-debug"
} else {
    "windows-msvc2019-release"
}
$BinDirName = if ($Configuration -eq "Debug") { "BinD" } else { "Bin" }
$SqliteDriverName = if ($Configuration -eq "Debug") { "qsqlited.dll" } else { "qsqlite.dll" }
$QtDebugSuffix = if ($Configuration -eq "Debug") { "d" } else { "" }
$BuildDir = Join-Path $SourceDir "build/$Preset"
$BinDir = Join-Path $BuildDir $BinDirName

if ($env:OS -ne "Windows_NT") {
    throw "This verification entry requires Windows."
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake 3.21 or newer is required."
}
$CMakeVersionText = (& cmake --version | Select-Object -First 1)
if ($CMakeVersionText -notmatch "([0-9]+\.[0-9]+\.[0-9]+)") {
    throw "Unable to determine the CMake version."
}
$CMakeVersion = [version]$Matches[1]
if ($CMakeVersion -lt [version]"3.21.0") {
    throw "CMake 3.21 or newer is required; found $CMakeVersion."
}
if ([string]::IsNullOrWhiteSpace($env:QTDIR)) {
    throw "Set QTDIR to the Qt 6.4 MSVC2019 x64 CMake prefix."
}
if (-not (Test-Path (Join-Path $env:QTDIR "lib/cmake/Qt6/Qt6Config.cmake"))) {
    throw "QTDIR does not contain lib/cmake/Qt6/Qt6Config.cmake."
}
$QMake = Join-Path $env:QTDIR "bin/qmake.exe"
if (-not (Test-Path $QMake)) {
    throw "QTDIR does not contain bin/qmake.exe."
}
$QtVersion = (& $QMake -query QT_VERSION).Trim()
if ($LASTEXITCODE -ne 0 -or $QtVersion -notmatch "^6\.4\.") {
    throw "Qt 6.4.x MSVC2019 x64 is required; found '$QtVersion'."
}
$env:PATH = (Join-Path $env:QTDIR "bin") + ";" + $env:PATH


$ConfigureArguments = @("--preset", $Preset)
if ($FullFeatures -and -not $SkipBuild) {
    $ConfigureArguments += @(Get-XVisionFullFeatureArguments $OpenCvRoot $OnnxRuntimeRoot)
}
Push-Location $SourceDir
try {
    & (Join-Path $SourceDir 'tests/TestWindowsPackaging.ps1')
    if (-not $SkipBuild) {
        & cmake @ConfigureArguments
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

        & cmake --build --preset $Preset
        if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }
    }
    if ($FullFeatures) {
        $BuildCache = Assert-XVisionFullFeatureCache $BuildDir
        foreach ($Option in @('XVISION_OPENCV_RUNTIME_DLL', 'XVISION_ONNXRUNTIME_RUNTIME_DLL')) {
            $RuntimeName = [System.IO.Path]::GetFileName($BuildCache[$Option])
            if (-not (Test-Path -LiteralPath (Join-Path $BinDir $RuntimeName) -PathType Leaf)) {
                throw "Enabled backend runtime is missing from build output: $RuntimeName"
            }
        }
    }

    $CTestInventoryLines = & ctest --test-dir $BuildDir -C $Configuration `
        --show-only=json-v1
    if ($LASTEXITCODE -ne 0) { throw "Unable to enumerate first-party tests." }
    $CTestInventory = ($CTestInventoryLines -join [Environment]::NewLine) |
        ConvertFrom-Json
    $DiscoveredTests = @($CTestInventory.tests | ForEach-Object { $_.name })
    foreach ($ExpectedTest in @(
        "projectxml",
        "camera",
        "systemplugin",
        "operator_catalog",
        "operator_catalog_rejections",
        "visionmaster_data",
        "visionmaster_sdk_cmake",
        "visionmaster_backend_boundary",
        "opencv_image_operators",
        "opencv_morphology_detectors",
        "opencv_template_rectification",
        "geometry_measurement",
        "detect_record",
        "communication_operators",
        "source_notification",
        "onnx_operators",
        "ui_appearance",
        "ui_responsiveness",
        "global_tools"
    )) {
        if ($DiscoveredTests -notcontains $ExpectedTest) {
            throw "Expected CTest entry is missing: $ExpectedTest"
        }
    }

    # Run the real CPU/ownership/persistence paths without private model assets.
    if ([string]::IsNullOrWhiteSpace($env:XVISION_ONNX_TEST_MODEL)) {
        $env:XVISION_ONNX_TEST_MODEL = Join-Path $SourceDir 'tests/fixtures/cpu-identity.onnx'
    }
    if ([string]::IsNullOrWhiteSpace($env:XVISION_ONNX_EXECUTION_TEST_MODEL)) {
        $env:XVISION_ONNX_EXECUTION_TEST_MODEL = Join-Path $SourceDir 'tests/fixtures/cpu-identity.onnx'
    }

    & ctest --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "First-party tests failed." }

    if ($SoakMinutes -gt 0) {
        $Deadline = [DateTime]::UtcNow.AddMinutes($SoakMinutes)
        $SoakRuns = 0
        do {
            & ctest --preset $Preset -R "^projectxml$"
            if ($LASTEXITCODE -ne 0) {
                throw "Acquisition project soak run $($SoakRuns + 1) failed."
            }
            $SoakRuns++
        } while ([DateTime]::UtcNow -lt $Deadline)
        Write-Host "Completed $SoakRuns projectxml soak run(s) in at least $SoakMinutes minute(s)."
    }
} finally {
    Pop-Location
}

$RequiredOutputs = @(
    (Join-Path $BinDir "XVision.exe"),
    (Join-Path $BinDir "XvCamera.dll"),
    (Join-Path $BinDir "XvFuncCollection/XvFuncSystem.dll"),
    (Join-Path $BinDir "sqldrivers/$SqliteDriverName"),
    (Join-Path $BinDir "Qt6Network$QtDebugSuffix.dll"),
    (Join-Path $BinDir "Qt6SerialPort$QtDebugSuffix.dll"),
    (Join-Path $BinDir "Qt6SerialBus$QtDebugSuffix.dll"),
    (Join-Path $BinDir "XWidget.dll"),
    (Join-Path $BinDir "XLog.dll"),
    (Join-Path $BinDir "XLanguage.dll"),
    (Join-Path $BinDir "AdsDocking.dll"),
    (Join-Path $BinDir "XFlowGraphics.dll"),
    (Join-Path $BinDir "XConcurrent.dll")
)
foreach ($OutputPath in $RequiredOutputs) {
    if (-not (Test-Path $OutputPath)) {
        throw "Expected build output is missing: $OutputPath"
    }
}

Write-Host "Windows $Configuration build and first-party tests passed."
Write-Host "CMake: $CMakeVersion; Qt: $QtVersion; no HALCON dependency."
Write-Host "Complete the manual persistence checklist in doc/开发工作/Windows构建与持久化验收.md."
