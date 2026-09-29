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
    # IMAGE_SUBSYSTEM_WINDOWS_GUI is 2. A console-subsystem build would reopen CMD.
    $PeBytes = [System.IO.File]::ReadAllBytes($Executable)
    $PeOffset = [BitConverter]::ToInt32($PeBytes, 0x3c)
    $Subsystem = [BitConverter]::ToUInt16($PeBytes, $PeOffset + 24 + 68)
    if ($Subsystem -ne 2) { throw "XVision.exe must use the Windows GUI subsystem; got $Subsystem." }
    $env:PATH = $SmokeDirectory + ';' + (Join-Path $env:SystemRoot 'System32') + ';' + $env:SystemRoot
    $env:QT_PLUGIN_PATH = $SmokeDirectory
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $SmokeDirectory 'platforms'
    $env:QT_QPA_PLATFORM = 'windows'
    $env:QT_LOGGING_TO_CONSOLE = '1'
    # Exercise the embedded engine through the shipped GUI executable and inherited pipes.
    $WorkerInfo = New-Object System.Diagnostics.ProcessStartInfo
    $WorkerInfo.FileName = $Executable
    $WorkerInfo.Arguments = '--script-worker'
    $WorkerInfo.WorkingDirectory = $SmokeDirectory
    $WorkerInfo.UseShellExecute = $false
    $WorkerInfo.CreateNoWindow = $true
    $WorkerInfo.RedirectStandardInput = $true
    $WorkerInfo.RedirectStandardOutput = $true
    $WorkerInfo.RedirectStandardError = $true
    $Worker = New-Object System.Diagnostics.Process
    $Worker.StartInfo = $WorkerInfo
    try {
        if (-not $Worker.Start()) { throw 'Unable to start packaged JavaScript worker.' }
        $Worker.StandardInput.Write('{"globals":{"count":1},"parameters":{},"results":{},"script":"globals.set(\"count\", globals.get(\"count\") + 1); console.log(\"worker-ok\");"}')
        $Worker.StandardInput.Close()
        if (-not $Worker.WaitForExit(10000)) { $Worker.Kill(); throw 'Packaged JavaScript worker timed out.' }
        $WorkerText = $Worker.StandardOutput.ReadToEnd()
        $WorkerText | Set-Content -LiteralPath (Join-Path $LogDirectory 'script-worker.json') -Encoding UTF8
        $WorkerResult = $WorkerText | ConvertFrom-Json
        if ($Worker.ExitCode -ne 0 -or -not $WorkerResult.ok -or $WorkerResult.globals.count -ne 2) {
            throw "Packaged JavaScript worker failed: $WorkerText"
        }
    } finally {
        if (-not $Worker.HasExited) { $Worker.Kill() }
        $Worker.Dispose()
    }
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
    Add-Type -AssemblyName System.Drawing
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class XVisionWindowCapture {
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr window, IntPtr dc, uint flags);
}
'@
    $Bounds = New-Object XVisionWindowCapture+Rect
    if ([XVisionWindowCapture]::GetWindowRect($Process.MainWindowHandle, [ref]$Bounds)) {
        $Bitmap = New-Object System.Drawing.Bitmap ($Bounds.Right - $Bounds.Left), ($Bounds.Bottom - $Bounds.Top)
        $Graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
        $Device = $Graphics.GetHdc()
        try { $Captured = [XVisionWindowCapture]::PrintWindow($Process.MainWindowHandle, $Device, 2) }
        finally { $Graphics.ReleaseHdc($Device) }
        try {
            if ($Captured) { $Bitmap.Save((Join-Path $LogDirectory 'technology-blue-main-window.png')) }
        } finally { $Graphics.Dispose(); $Bitmap.Dispose() }
    }
    'GUI subsystem=2; embedded JavaScript worker passed; main window opened using only packaged files and Windows system directories on PATH.' |
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
