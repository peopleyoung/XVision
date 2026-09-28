param(
    [Parameter(Mandatory = $true)][string]$ArchivePath,
    [string]$LogDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build/windows-msvc2019-release/Testing/ApplicationSmoke')
)

$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT') { throw 'The packaged application startup test requires Windows.' }
if (-not (Test-Path -LiteralPath $ArchivePath -PathType Leaf)) { throw "Package archive is missing: $ArchivePath" }
$ArchivePath = (Resolve-Path -LiteralPath $ArchivePath).ProviderPath
$LogDirectory = [System.IO.Path]::GetFullPath($LogDirectory)
New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
$SmokeDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ('xvision-app-smoke-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $SmokeDirectory | Out-Null
$SavedEnvironment = @{}
foreach ($Name in @('PATH', 'QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QT_QPA_PLATFORM', 'QT_LOGGING_TO_CONSOLE')) {
    $SavedEnvironment[$Name] = [Environment]::GetEnvironmentVariable($Name)
}
$Process = $null
try {
    Expand-Archive -LiteralPath $ArchivePath -DestinationPath $SmokeDirectory
    $Executable = Join-Path $SmokeDirectory 'XVision.exe'
    if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) { throw 'The archive has no root XVision.exe.' }
    $env:PATH = $SmokeDirectory + ';' + (Join-Path $env:SystemRoot 'System32') + ';' + $env:SystemRoot
    $env:QT_PLUGIN_PATH = $SmokeDirectory
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $SmokeDirectory 'platforms'
    $env:QT_QPA_PLATFORM = 'windows'
    $env:QT_LOGGING_TO_CONSOLE = '1'
    $StartArguments = @{
        FilePath = $Executable
        WorkingDirectory = $SmokeDirectory
        PassThru = $true
        RedirectStandardOutput = (Join-Path $LogDirectory 'stdout.log')
        RedirectStandardError = (Join-Path $LogDirectory 'stderr.log')
    }
    $Process = Start-Process @StartArguments
    $Deadline = [DateTime]::UtcNow.AddSeconds(30)
    $Ready = $false
    do {
        Start-Sleep -Milliseconds 500
        $Process.Refresh()
        if ($Process.HasExited) { throw "Packaged XVision exited during startup: $($Process.ExitCode). See application smoke logs." }
        if ($Process.MainWindowHandle -ne [IntPtr]::Zero -and $Process.MainWindowTitle -eq 'XVision') {
            $Ready = $true
            break
        }
    } while ([DateTime]::UtcNow -lt $Deadline)
    if (-not $Ready) { throw 'Packaged XVision did not open its main window within 30 seconds.' }
    Start-Sleep -Seconds 3
    $Process.Refresh()
    if ($Process.HasExited) { throw "Packaged XVision exited after opening its window: $($Process.ExitCode)." }
    'Packaged XVision main window opened successfully with only packaged files and Windows system directories on PATH.' |
        Set-Content -LiteralPath (Join-Path $LogDirectory 'result.txt') -Encoding UTF8
    Write-Host 'Packaged XVision main window opened successfully.'
} finally {
    if ($Process -and -not $Process.HasExited) {
        $null = $Process.CloseMainWindow()
        if (-not $Process.WaitForExit(5000)) {
            Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
            $null = $Process.WaitForExit(5000)
        }
    }
    foreach ($Name in $SavedEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($Name, $SavedEnvironment[$Name])
    }
    Remove-Item -LiteralPath $SmokeDirectory -Recurse -Force
}
