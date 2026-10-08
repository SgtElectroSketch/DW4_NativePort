[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$FilePath,
    [string[]]$ArgumentList = @(),
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 600,
    [string]$WorkingDirectory = (Get-Location).Path,
    [string]$LogDirectory = ''
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrEmpty($LogDirectory)) {
    $LogDirectory = Join-Path $PSScriptRoot '..\..\build\validation'
}
$logRoot = [System.IO.Path]::GetFullPath($LogDirectory)
[System.IO.Directory]::CreateDirectory($logRoot) | Out-Null
$logId = [Guid]::NewGuid().ToString('N')
$stdoutPath = Join-Path $logRoot "$logId.stdout.log"
$stderrPath = Join-Path $logRoot "$logId.stderr.log"
$process = $null
$timedOut = $false
$exitCode = 1

try {
    $startParameters = @{
        FilePath = $FilePath
        WorkingDirectory = $WorkingDirectory
        RedirectStandardOutput = $stdoutPath
        RedirectStandardError = $stderrPath
        NoNewWindow = $true
        PassThru = $true
    }
    if ($ArgumentList.Count -gt 0) {
        $startParameters.ArgumentList = $ArgumentList
    }
    $process = Start-Process @startParameters
    [void]$process.Handle
    $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
    if ($timedOut) {
        & "$env:SystemRoot\System32\taskkill.exe" /PID $process.Id /T /F | Out-Null
        $exitCode = 124
    } else {
        $exitCode = $process.ExitCode
    }
} finally {
    if ($null -ne $process) {
        $process.Dispose()
    }
    foreach ($logPath in @($stdoutPath, $stderrPath)) {
        if (Test-Path -LiteralPath $logPath) {
            Get-Content -LiteralPath $logPath -Tail 120
        }
    }
    Write-Output "Process logs: $stdoutPath ; $stderrPath"
}

if ($timedOut) {
    Write-Output "Process exceeded $TimeoutSeconds seconds and was terminated."
}
Write-Output "Process exit code: $exitCode"
exit $exitCode