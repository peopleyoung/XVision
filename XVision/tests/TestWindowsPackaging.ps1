$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$SourceDirectory = Split-Path -Parent $PSScriptRoot
. (Join-Path $SourceDirectory 'scripts/WindowsBuild.Common.ps1')
$Checks = 0

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
    $script:Checks++
}

function Assert-Fails {
    param([scriptblock]$Action, [string]$ExpectedMessage)
    $Message = ''
    try { & $Action | Out-Null } catch { $Message = $_.Exception.Message }
    Assert-True ($Message.Contains($ExpectedMessage)) "Expected '$ExpectedMessage'; got '$Message'."
}

$TemporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ('xvision-package-tests-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $TemporaryDirectory | Out-Null
try {
    foreach ($Script in @('Package-WindowsBuild.ps1', 'Verify-WindowsBuild.ps1', 'WindowsBuild.Common.ps1', 'Prepare-WindowsBuild.ps1', 'Test-PackagedWindowsApp.ps1')) {
        $Tokens = $null
        $ParseErrors = $null
        $ScriptPath = Join-Path $SourceDirectory "scripts/$Script"
        $null = [System.Management.Automation.Language.Parser]::ParseFile($ScriptPath, [ref]$Tokens, [ref]$ParseErrors)
        Assert-True ($ParseErrors.Count -eq 0) "$Script has parse errors: $ParseErrors"
    }

    $OpenCvRoot = Join-Path $TemporaryDirectory 'OpenCV SDK'
    $OnnxRuntimeRoot = Join-Path $TemporaryDirectory 'ONNX SDK'
    New-Item -ItemType Directory -Path $OpenCvRoot, $OnnxRuntimeRoot | Out-Null
    Assert-Fails { Get-XVisionFullFeatureArguments '' $OnnxRuntimeRoot } '-OpenCvRoot'
    Assert-Fails { Get-XVisionFullFeatureArguments $OpenCvRoot '' } '-OnnxRuntimeRoot'
    Assert-Fails { Get-XVisionFullFeatureArguments (Join-Path $TemporaryDirectory 'missing') $OnnxRuntimeRoot } '-OpenCvRoot'
    $Arguments = @(Get-XVisionFullFeatureArguments $OpenCvRoot $OnnxRuntimeRoot)
    Assert-True ($Arguments -contains '-DXVISION_ENABLE_OPENCV=ON') 'OpenCV must be enabled.'
    Assert-True ($Arguments -contains '-DXVISION_ENABLE_ONNXRUNTIME=ON') 'ONNX Runtime must be enabled.'
    Assert-True ($Arguments -contains "-DXVISION_OPENCV_ROOT=$OpenCvRoot") 'SDK paths with spaces must remain one argument.'
    Assert-True ($Arguments -contains "-DXVISION_ONNXRUNTIME_ROOT=$OnnxRuntimeRoot") 'ONNX SDK paths with spaces must remain one argument.'
    Push-Location $TemporaryDirectory
    try {
        $RelativeArguments = @(Get-XVisionFullFeatureArguments 'OpenCV SDK' 'ONNX SDK')
        Assert-True ($RelativeArguments -contains "-DXVISION_OPENCV_ROOT=$OpenCvRoot") 'Relative SDK paths must resolve against the caller location.'
    } finally {
        Pop-Location
    }

    $CachePath = Join-Path $TemporaryDirectory 'CMakeCache.txt'
    Assert-Fails { Assert-XVisionFullFeatureCache $TemporaryDirectory } 'Missing CMake cache'
    $OpenCvDll = Join-Path $OpenCvRoot 'opencv_world4100.dll'
    $OnnxDll = Join-Path $OnnxRuntimeRoot 'onnxruntime.dll'
    Set-Content -LiteralPath $OpenCvDll, $OnnxDll -Value 'runtime fixture'
    $RequiredOptions = @('BUILD_TESTING', 'XVISION_BUILD_COMMON_USING', 'XVISION_BUILD_SYSTEM_PLUGIN',
        'XVISION_ENABLE_BREAKPAD', 'XVISION_ENABLE_OPENCV', 'XVISION_ENABLE_ONNXRUNTIME')
    $ValidCache = @($RequiredOptions | ForEach-Object { $_ + ':BOOL=ON' }) + @(
        "XVISION_OPENCV_RUNTIME_DLL:FILEPATH=$OpenCvDll",
        "XVISION_ONNXRUNTIME_RUNTIME_DLL:FILEPATH=$OnnxDll"
    )
    Set-Content -LiteralPath $CachePath -Value $ValidCache
    $Cache = Assert-XVisionFullFeatureCache $TemporaryDirectory
    Assert-True ($Cache['XVISION_OPENCV_RUNTIME_DLL'] -eq $OpenCvDll) 'Cache paths must survive parsing.'
    foreach ($Option in $RequiredOptions) {
        Set-Content -LiteralPath $CachePath -Value ($ValidCache.Replace($Option + ':BOOL=ON', $Option + ':BOOL=OFF'))
        Assert-Fails { Assert-XVisionFullFeatureCache $TemporaryDirectory } ($Option + '=ON')
        Set-Content -LiteralPath $CachePath -Value @($ValidCache | Where-Object { -not $_.StartsWith($Option + ':') })
        Assert-Fails { Assert-XVisionFullFeatureCache $TemporaryDirectory } ($Option + '=ON')
    }
    Set-Content -LiteralPath $CachePath -Value $ValidCache
    Remove-Item -LiteralPath $OnnxDll
    Assert-Fails { Assert-XVisionFullFeatureCache $TemporaryDirectory } 'XVISION_ONNXRUNTIME_RUNTIME_DLL'
    Set-Content -LiteralPath $OnnxDll -Value 'runtime fixture'
    Remove-Item -LiteralPath $OpenCvDll
    Assert-Fails { Assert-XVisionFullFeatureCache $TemporaryDirectory } 'XVISION_OPENCV_RUNTIME_DLL'

    foreach ($Configuration in @('Debug', 'Release')) {
        $DeploymentArguments = @(Get-XVisionQtDeploymentArguments $Configuration $TemporaryDirectory)
        Assert-True ($DeploymentArguments -contains '--sql') 'Qt SQL module must be deployed.'
        $Suffix = if ($Configuration -eq 'Debug') { 'd' } else { '' }
        foreach ($Module in @('Concurrent', 'StateMachine')) {
            Assert-True ($DeploymentArguments -contains (Join-Path $TemporaryDirectory "Qt6${Module}${Suffix}.dll")) "Declared Qt $Module module must be scanned."
        }
        Assert-True ($DeploymentArguments -notcontains 'sqlite') 'sqlite is not a positional executable for windeployqt.'
        Assert-True ($DeploymentArguments -contains (Join-Path $TemporaryDirectory 'XVision.exe')) 'Application must be scanned.'
        Assert-True ($DeploymentArguments -contains (Join-Path $TemporaryDirectory 'XvFuncCollection/XvFuncSystem.dll')) 'Dynamically loaded plugin must be scanned.'
    }
    $DebugFiles = @(Get-XVisionQtRuntimeFiles Debug)
    Assert-True ($DebugFiles -contains 'platforms/qwindowsd.dll') 'Debug platform plugin suffix is required.'
    Assert-True ($DebugFiles -contains 'sqldrivers/qsqlited.dll') 'Debug SQLite driver suffix is required.'
    Assert-True ($DebugFiles -contains 'Qt6Sqld.dll') 'Debug Qt SQL runtime is required.'
    Assert-True ($DebugFiles -notcontains 'platforms/qwindows.dll') 'Release platform plugin must not satisfy Debug.'
    $ReleaseFiles = @(Get-XVisionQtRuntimeFiles Release)
    Assert-True ($ReleaseFiles -contains 'platforms/qwindows.dll') 'Release platform plugin is required.'
    Assert-True ($ReleaseFiles -contains 'sqldrivers/qsqlite.dll') 'Release SQLite driver is required.'
    Assert-True ($ReleaseFiles -contains 'Qt6Sql.dll') 'Release Qt SQL runtime is required.'
    Assert-True ($ReleaseFiles -contains 'Qt6SerialBus.dll') 'Plugin-only Qt modules must be checked.'
    Write-Host "Windows packaging contract checks passed: $Checks"
} finally {
    Remove-Item -LiteralPath $TemporaryDirectory -Recurse -Force
}
