param(
    [Parameter(Mandatory = $true)]
    [string]$SdkDirectory
)

$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') {
    throw 'SDK preparation requires Windows x64.'
}
if (-not [Environment]::Is64BitProcess) {
    throw 'Run SDK preparation from a 64-bit PowerShell process.'
}
New-Item -ItemType Directory -Path $SdkDirectory -Force | Out-Null
$SdkDirectory = (Resolve-Path -LiteralPath $SdkDirectory).ProviderPath
$DownloadDirectory = Join-Path $env:TEMP 'xvision-build-downloads'
New-Item -ItemType Directory -Path $DownloadDirectory -Force | Out-Null

function Invoke-BuildTool {
    param([string]$Command, [string[]]$Arguments)
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Build tool failed ($LASTEXITCODE): $Command"
    }
}

function Get-BuildDownload {
    param([string]$Url, [string]$Name)
    $Path = Join-Path $DownloadDirectory $Name
    Invoke-WebRequest -Uri $Url -OutFile $Path -MaximumRetryCount 3 -RetryIntervalSec 5
    Write-Host "SHA256 $Name $((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash)"
    return $Path
}

# The application presets deliberately use the supported VS 2019 generator.
$VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $VsWhere)) {
    throw 'The Windows runner must provide Visual Studio Installer/vswhere.exe.'
}
$Vs2019 = & $VsWhere -products '*' -version '[16.0,17.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $Vs2019) {
    $Installer = Get-BuildDownload 'https://aka.ms/vs/16/release/vs_buildtools.exe' 'vs2019-buildtools.exe'
    $Signature = Get-AuthenticodeSignature -LiteralPath $Installer
    if ($Signature.Status -ne 'Valid' -or $Signature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') {
        throw 'Visual Studio bootstrapper must have a valid Microsoft signature.'
    }
    $Install = Start-Process -FilePath $Installer -Wait -PassThru -ArgumentList @(
        '--quiet', '--wait', '--norestart', '--nocache',
        '--installPath', 'C:\XVisionBuildTools2019',
        '--add', 'Microsoft.VisualStudio.Workload.VCTools', '--includeRecommended'
    )
    if ($Install.ExitCode -notin @(0, 3010)) {
        throw "Visual Studio 2019 installation failed: $($Install.ExitCode)"
    }
    $Vs2019 = & $VsWhere -products '*' -version '[16.0,17.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $Vs2019) { throw 'Visual Studio 2019 C++ tools were not installed.' }
}

$QtBase = Join-Path $SdkDirectory 'Qt'
$QtDirectory = Join-Path $QtBase '6.4.0/msvc2019_64'
if (-not (Test-Path -LiteralPath (Join-Path $QtDirectory 'bin/qmake.exe'))) {
    Invoke-BuildTool 'python' @(
        '-m', 'aqt', 'install-qt', 'windows', 'desktop', '6.4.0', 'win64_msvc2019_64',
        '--outputdir', $QtBase, '--modules', 'qtserialport', 'qtserialbus', 'qtsvg'
    )
}

$OpenCvRoot = Join-Path $SdkDirectory 'opencv'
if (-not (Test-Path -LiteralPath (Join-Path $OpenCvRoot 'bin/opencv_world4100.dll'))) {
    # Official prebuilt OpenCV packages omit required contrib modules. Build
    # world + contrib together so xfeatures2d and dnn_superres are present.
    $OpenCvZip = Get-BuildDownload 'https://github.com/opencv/opencv/archive/refs/tags/4.10.0.zip' 'opencv-4.10.0.zip'
    $ContribZip = Get-BuildDownload 'https://github.com/opencv/opencv_contrib/archive/refs/tags/4.10.0.zip' 'opencv-contrib-4.10.0.zip'
    Expand-Archive -LiteralPath $OpenCvZip -DestinationPath $DownloadDirectory -Force
    Expand-Archive -LiteralPath $ContribZip -DestinationPath $DownloadDirectory -Force
    $OpenCvSource = Join-Path $DownloadDirectory 'opencv-4.10.0'
    $ContribModules = Join-Path $DownloadDirectory 'opencv_contrib-4.10.0/modules'
    $OpenCvBuild = Join-Path $DownloadDirectory 'opencv-build'
    $OpenCvInstall = Join-Path $DownloadDirectory 'opencv-install'
    Invoke-BuildTool 'cmake' @(
        '-S', $OpenCvSource, '-B', $OpenCvBuild, '-G', 'Visual Studio 16 2019', '-A', 'x64',
        "-DCMAKE_INSTALL_PREFIX=$OpenCvInstall", "-DOPENCV_EXTRA_MODULES_PATH=$ContribModules",
        '-DBUILD_LIST=core,imgproc,imgcodecs,features2d,xfeatures2d,calib3d,photo,objdetect,stitching,ml,dnn,dnn_superres,video,videoio,world',
        '-DBUILD_opencv_world=ON', '-DBUILD_TESTS=OFF', '-DBUILD_PERF_TESTS=OFF',
        '-DBUILD_EXAMPLES=OFF', '-DBUILD_DOCS=OFF', '-DBUILD_opencv_apps=OFF',
        '-DBUILD_opencv_python2=OFF', '-DBUILD_opencv_python3=OFF', '-DBUILD_JAVA=OFF',
        '-DWITH_CUDA=OFF', '-DWITH_QT=OFF', '-DWITH_FFMPEG=ON'
    )
    Invoke-BuildTool 'cmake' @('--build', $OpenCvBuild, '--config', 'Release', '--target', 'INSTALL', '--parallel', '4')
    New-Item -ItemType Directory -Path $OpenCvRoot, (Join-Path $OpenCvRoot 'lib'), (Join-Path $OpenCvRoot 'bin') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $OpenCvInstall 'include') -Destination $OpenCvRoot -Recurse -Force
    $WorldLib = @(Get-ChildItem -LiteralPath $OpenCvInstall -Recurse -File -Filter 'opencv_world4100.lib')
    $WorldDll = @(Get-ChildItem -LiteralPath $OpenCvInstall -Recurse -File -Filter 'opencv_world4100.dll')
    if ($WorldLib.Count -ne 1 -or $WorldDll.Count -ne 1) {
        throw 'OpenCV installation did not produce one Release world import library and DLL.'
    }
    Copy-Item -LiteralPath $WorldLib[0].FullName -Destination (Join-Path $OpenCvRoot 'lib')
    Copy-Item -LiteralPath $WorldDll[0].FullName -Destination (Join-Path $OpenCvRoot 'bin')
    Get-ChildItem -LiteralPath $OpenCvInstall -Recurse -File -Filter 'opencv_videoio_ffmpeg*.dll' |
        Copy-Item -Destination (Join-Path $OpenCvRoot 'bin') -Force
}

$OnnxRoot = Join-Path $SdkDirectory 'onnxruntime'
if (-not (Test-Path -LiteralPath (Join-Path $OnnxRoot 'lib/onnxruntime.dll'))) {
    $OnnxZip = Get-BuildDownload 'https://github.com/microsoft/onnxruntime/releases/download/v1.18.1/onnxruntime-win-x64-1.18.1.zip' 'onnxruntime-1.18.1.zip'
    Expand-Archive -LiteralPath $OnnxZip -DestinationPath $DownloadDirectory -Force
    $OnnxExtracted = Join-Path $DownloadDirectory 'onnxruntime-win-x64-1.18.1'
    New-Item -ItemType Directory -Path $OnnxRoot -Force | Out-Null
    Copy-Item -Path (Join-Path $OnnxExtracted '*') -Destination $OnnxRoot -Recurse -Force
}

$Values = @{ QTDIR = $QtDirectory; XVISION_OPENCV_ROOT = $OpenCvRoot; XVISION_ONNXRUNTIME_ROOT = $OnnxRoot }
foreach ($Name in $Values.Keys) {
    Set-Item -LiteralPath "Env:$Name" -Value $Values[$Name]
    if ($env:GITHUB_ENV) {
        "$Name=$($Values[$Name])" | Out-File -LiteralPath $env:GITHUB_ENV -Append -Encoding utf8
    }
}
Write-Host 'Windows SDKs prepared. Next: Package-WindowsBuild.ps1 -Configuration Release'
