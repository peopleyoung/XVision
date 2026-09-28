param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$OpenCvRoot = $env:XVISION_OPENCV_ROOT,
    [string]$OnnxRuntimeRoot = $env:XVISION_ONNXRUNTIME_ROOT,
    [string]$OutputDirectory = "",
    [switch]$SkipBuild,
    [switch]$SkipTests,
    [switch]$KeepStaging
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
$QtDebugSuffix = if ($Configuration -eq "Debug") { "d" } else { "" }
$BuildDir = Join-Path $SourceDir "build/$Preset"
$BinDir = Join-Path $BuildDir $BinDirName
$PackageName = "XVision-$Configuration-win64"

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $SourceDir "dist"
}
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
$PackageDirectory = Join-Path $OutputDirectory $PackageName
$ArchivePath = Join-Path $OutputDirectory "$PackageName.zip"

function Assert-File {
    param([string]$Path, [string]$Description)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing ${Description}: $Path"
    }
}

function Invoke-Checked {
    param(
        [string]$Command,
        [string[]]$Arguments,
        [string]$FailureMessage
    )
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw $FailureMessage
    }
}

if ($env:OS -ne "Windows_NT") {
    throw "This packaging entry requires Windows x64 with the MSVC toolchain."
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
Assert-File (Join-Path $env:QTDIR "lib/cmake/Qt6/Qt6Config.cmake") "Qt6 CMake configuration"
$QMake = Join-Path $env:QTDIR "bin/qmake.exe"
$Windeployqt = Join-Path $env:QTDIR "bin/windeployqt.exe"
Assert-File $QMake "Qt qmake"
Assert-File $Windeployqt "Qt windeployqt"
$QtVersion = (& $QMake -query QT_VERSION).Trim()
if ($LASTEXITCODE -ne 0 -or $QtVersion -notmatch "^6\.4\.") {
    throw "Qt 6.4.x MSVC2019 x64 is required; found '$QtVersion'."
}
$env:PATH = (Join-Path $env:QTDIR "bin") + ";" + $env:PATH


if (-not $SkipBuild) {
    $FullFeatureArguments = @(Get-XVisionFullFeatureArguments $OpenCvRoot $OnnxRuntimeRoot)
    $OpenCvRoot = (Resolve-Path -LiteralPath $OpenCvRoot).ProviderPath
    $OnnxRuntimeRoot = (Resolve-Path -LiteralPath $OnnxRuntimeRoot).ProviderPath
    $VerifyScript = Join-Path $PSScriptRoot "Verify-WindowsBuild.ps1"
    $VerifyArguments = @(
        "-Configuration", $Configuration, "-FullFeatures",
        "-OpenCvRoot", $OpenCvRoot, "-OnnxRuntimeRoot", $OnnxRuntimeRoot
    )
    if ($SkipTests) {
        Push-Location $SourceDir
        try {
            Invoke-Checked "cmake" (@("--preset", $Preset) + $FullFeatureArguments) "CMake configure failed."
            Invoke-Checked "cmake" @("--build", "--preset", $Preset) "CMake build failed."
        } finally {
            Pop-Location
        }
    } else {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $VerifyScript @VerifyArguments
        if ($LASTEXITCODE -ne 0) {
            throw "Windows build or first-party tests failed."
        }
    }
}

# SkipBuild reuses binaries, but must never accept a reduced-feature cache.
$BuildCache = Assert-XVisionFullFeatureCache $BuildDir
if ($SkipBuild -and -not $SkipTests) {
    $VerifyScript = Join-Path $PSScriptRoot "Verify-WindowsBuild.ps1"
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $VerifyScript `
        -Configuration $Configuration -FullFeatures -SkipBuild
    if ($LASTEXITCODE -ne 0) {
        throw "First-party tests for the existing Windows build failed."
    }
}
$SdkRuntimeNames = @(
    [System.IO.Path]::GetFileName($BuildCache['XVISION_OPENCV_RUNTIME_DLL']),
    [System.IO.Path]::GetFileName($BuildCache['XVISION_ONNXRUNTIME_RUNTIME_DLL'])
)
foreach ($RuntimeName in $SdkRuntimeNames) {
    Assert-File (Join-Path $BinDir $RuntimeName) "enabled backend runtime $RuntimeName"
}

Assert-File (Join-Path $BinDir "XVision.exe") "XVision executable"
Assert-File (Join-Path $BinDir "XvCamera.dll") "XvCamera runtime"
Assert-File (Join-Path $BinDir "XvFuncCollection/XvFuncSystem.dll") "system operator plugin"

$RequiredCommonDlls = @(
    "XWidget.dll",
    "XLog.dll",
    "XLanguage.dll",
    "AdsDocking.dll",
    "XFlowGraphics.dll",
    "XConcurrent.dll"
)
foreach ($DllName in $RequiredCommonDlls) {
    Assert-File (Join-Path $BinDir $DllName) "CommonUsing runtime $DllName"
}

if (Test-Path -LiteralPath $PackageDirectory) {
    Remove-Item -LiteralPath $PackageDirectory -Recurse -Force
}
if (Test-Path -LiteralPath $ArchivePath) {
    Remove-Item -LiteralPath $ArchivePath -Force
}
New-Item -ItemType Directory -Path $PackageDirectory -Force | Out-Null

# Keep every application DLL in the root and preserve the plugin directory.
# OpenCV and ONNX Runtime are required by this full-feature package entry.
Get-ChildItem -LiteralPath $BinDir -File -Filter "*.dll" |
    Where-Object { $_.Name -notmatch '^halcon.*\.dll$' } |
    Copy-Item -Destination $PackageDirectory -Force
$PluginSource = Join-Path $BinDir "XvFuncCollection"
$PluginDestination = Join-Path $PackageDirectory "XvFuncCollection"
New-Item -ItemType Directory -Path $PluginDestination -Force | Out-Null
Get-ChildItem -LiteralPath $PluginSource -File -Filter "*.dll" |
    Copy-Item -Destination $PluginDestination -Force
Copy-Item -LiteralPath (Join-Path $BinDir "XVision.exe") -Destination $PackageDirectory -Force

# Release linking can remove references to declared Qt modules. Stage every
# required module and scan it so the package contract does not depend on that.
foreach ($RelativePath in @(Get-XVisionQtRuntimeFiles $Configuration)) {
    if ([System.IO.Path]::GetFileName($RelativePath) -ne $RelativePath) { continue }
    $QtRuntimeSource = Join-Path $env:QTDIR ("bin/" + $RelativePath)
    Assert-File $QtRuntimeSource "declared Qt runtime $RelativePath"
    Copy-Item -LiteralPath $QtRuntimeSource -Destination $PackageDirectory -Force
}

$WindeployqtArguments = @(Get-XVisionQtDeploymentArguments $Configuration $PackageDirectory)
Invoke-Checked $Windeployqt $WindeployqtArguments "Qt runtime deployment failed."

# Qt's Release deployment only adds the redistributable installer. Include the
# x64 CRT DLLs beside the EXE so startup does not depend on running that installer.
$MsvcRuntimeFiles = @(Get-XVisionMsvcRuntimeFiles $Configuration)
$MsvcRuntimeFiles | Copy-Item -Destination $PackageDirectory -Force

$PackageReadmePath = Join-Path $PackageDirectory "PACKAGE-README.txt"
@(
    "XVision Windows x64",
    "",
    "Start the application with XVision.exe.",
    "Keep XvFuncCollection beside XVision.exe; it contains the operator plugin.",
    "This build contains no HALCON operators or HALCON runtime dependency.",
    "Camera drivers and real serial/Modbus devices are deployment-machine dependencies."
) | Set-Content -LiteralPath $PackageReadmePath -Encoding UTF8

# windeployqt handles Qt dependencies; these checks cover the project-specific
# deployment contract and the plugin lookup path used by XvPluginManager.
$RequiredPackageFiles = @(
    "XVision.exe",
    "XvCore.dll",
    "XvData.dll",
    "XvDisplay.dll",
    "XvTokenMsg.dll",
    "XvUtils.dll",
    "XvCamera.dll",
    "XvFuncCollection/XvFuncSystem.dll"
) + $RequiredCommonDlls + $SdkRuntimeNames + @(Get-XVisionQtRuntimeFiles $Configuration) + @($MsvcRuntimeFiles | ForEach-Object { $_.Name })
foreach ($RelativePath in $RequiredPackageFiles) {
    Assert-File (Join-Path $PackageDirectory $RelativePath) "packaged runtime file $RelativePath"
}

if (Get-ChildItem -LiteralPath $PackageDirectory -Recurse -File -Filter "halcon*.dll") {
    throw "HALCON runtime must not be included in the license-free package."
}

if (-not $SkipTests) {
    # Run from the staging root with no build/Qt SDK directories on PATH.
    # This catches DLLs that existed during CTest but were omitted from the ZIP.
    $PluginTestSource = Join-Path $BuildDir "tests/$Configuration/XvSystemPluginTests.exe"
    $QtTestSource = Join-Path $env:QTDIR "bin/Qt6Test${QtDebugSuffix}.dll"
    Assert-File $PluginTestSource "plugin test executable"
    Assert-File $QtTestSource "Qt Test runtime"
    $PluginTest = Join-Path $PackageDirectory 'XvSystemPluginTests.exe'
    $QtTestRuntime = Join-Path $PackageDirectory "Qt6Test${QtDebugSuffix}.dll"
    $HadQtTestRuntime = Test-Path -LiteralPath $QtTestRuntime -PathType Leaf
    $SavedPath = $env:PATH
    $SavedPluginPath = $env:QT_PLUGIN_PATH
    try {
        Copy-Item -LiteralPath $PluginTestSource -Destination $PluginTest
        if (-not $HadQtTestRuntime) {
            Copy-Item -LiteralPath $QtTestSource -Destination $QtTestRuntime
        }
        $env:PATH = $PackageDirectory + ';' + (Join-Path $env:SystemRoot 'System32') + ';' + $env:SystemRoot
        $env:QT_PLUGIN_PATH = $PackageDirectory
        Push-Location $PackageDirectory
        try {
            $MatrixPath = Join-Path (Split-Path -Parent $SourceDir) 'doc/开发工作/VisionMaster算子兼容矩阵.csv'
            Invoke-Checked $PluginTest @((Join-Path $PluginDestination 'XvFuncSystem.dll'), $MatrixPath) `
                "Packaged plugin failed to load or validate its 39 roles / 126 presets."
        } finally {
            Pop-Location
        }
    } finally {
        $env:PATH = $SavedPath
        $env:QT_PLUGIN_PATH = $SavedPluginPath
        if (Test-Path -LiteralPath $PluginTest) {
            Remove-Item -LiteralPath $PluginTest -Force
        }
        if (-not $HadQtTestRuntime -and (Test-Path -LiteralPath $QtTestRuntime)) {
            Remove-Item -LiteralPath $QtTestRuntime -Force
        }
    }
}

# Retain the exact source and license notices for the embedded Qt translations.
$TranslationSources = Join-Path $SourceDir 'XVision/Res/translations'
$TranslationNotices = Join-Path $PackageDirectory 'licenses/qt-translations'
New-Item -ItemType Directory -Path $TranslationNotices -Force | Out-Null
foreach ($Name in @('NOTICE.md', 'GPL-3.0-only.txt', 'Qt-GPL-exception-1.0.txt',
                    'qtbase_zh_CN.ts', 'xvision_zh_CN.ts')) {
    $Source = Join-Path $TranslationSources $Name
    Assert-File $Source "Chinese translation source or notice $Name"
    Copy-Item -LiteralPath $Source -Destination $TranslationNotices
}

$GitCommit = "unknown"
if (Get-Command git -ErrorAction SilentlyContinue) {
    $GitCommit = (& git -C $SourceDir rev-parse HEAD 2>$null).Trim()
    if ([string]::IsNullOrWhiteSpace($GitCommit)) {
        $GitCommit = "unknown"
    }
}
$ManifestPath = Join-Path $PackageDirectory "PACKAGE-MANIFEST.txt"
$ManifestLines = @(
    "Package: $PackageName",
    "Configuration: $Configuration",
    "Architecture: x64",
    "MSVC runtime: app-local x64 CRT DLLs",
    "Qt: $QtVersion",
    "CMake: $CMakeVersion",
    "HALCON dependency: none",
    "OpenCV: enabled ($($SdkRuntimeNames[0]))",
    "ONNX Runtime: enabled ($($SdkRuntimeNames[1]))",
    "First-party tests: $(if ($SkipTests) { 'SKIPPED - package not runtime-verified' } else { 'passed' })",
    "Packaged plugin test: $(if ($SkipTests) { 'SKIPPED' } else { 'passed (39 roles / 126 presets)' })",
    "Git commit: $GitCommit",
    "",
    "Files and SHA256:"
)
foreach ($File in Get-ChildItem -LiteralPath $PackageDirectory -File -Recurse |
    Sort-Object FullName) {
    $Hash = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash
    $RelativePath = $File.FullName.Substring($PackageDirectory.Length + 1)
    $ManifestLines += "$Hash  $RelativePath"
}
Set-Content -LiteralPath $ManifestPath -Value $ManifestLines -Encoding UTF8

Compress-Archive -Path (Join-Path $PackageDirectory "*") -DestinationPath $ArchivePath -CompressionLevel Optimal

Write-Host "Windows package created: $ArchivePath"
Write-Host "Staged directory: $PackageDirectory"
Write-Host "Qt: $QtVersion; CMake: $CMakeVersion; Git: $GitCommit"
Write-Host "No HALCON runtime license is required by this build."

if (-not $KeepStaging) {
    Remove-Item -LiteralPath $PackageDirectory -Recurse -Force
    Write-Host "Staged directory removed; ZIP archive retained."
}
