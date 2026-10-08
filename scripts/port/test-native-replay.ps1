[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output = Join-Path $root ('build\replay-smoke-' + [Guid]::NewGuid().ToString('N'))
$runner = Join-Path $PSScriptRoot 'run-bounded-process.ps1'
$tests = Join-Path $root "build\$Configuration\DW4.Tests.exe"
$desktop = Join-Path $root "build\$Configuration\DW4.Desktop.exe"
$originalExport = $env:DW4_REPLAY_SMOKE_OUTPUT
$encoding = [System.Text.UTF8Encoding]::new($false)
$catalogs = [ordered]@{
    'text/dialogue.json' = '{"schema_version":1,"messages":[]}'
    'maps/index.json' = '{"schema_version":1,"locations":[]}'
    'sprites/field/index.json' = '{"schema_version":1,"sets":[]}'
    'monsters/index.json' = '{"schema_version":1,"monsters":[]}'
    'graphics/index.json' = '{"schema_version":1,"chr":[],"fonts":[]}'
    'screens/index.json' = '{"schema_version":1,"scenes":[]}'
    'audio/index.json' = '{"schema_version":1,"music":[],"sfx":[]}'
}

function Invoke-Bounded([string]$Executable, [string]$NativeArguments, [int]$ExpectedExitCode) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -FilePath $Executable -ArgumentList $NativeArguments `
        -WorkingDirectory $root -TimeoutSeconds 30
    if ($LASTEXITCODE -ne $ExpectedExitCode) {
        throw "Expected exit $ExpectedExitCode but received $LASTEXITCODE from $Executable"
    }
}

try {
    [System.IO.Directory]::CreateDirectory($output) | Out-Null
    foreach ($relativePath in $catalogs.Keys) {
        $path = Join-Path $output $relativePath
        [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($path)) | Out-Null
        [System.IO.File]::WriteAllText($path, $catalogs[$relativePath], $encoding)
    }
    $env:DW4_REPLAY_SMOKE_OUTPUT = $output
    Invoke-Bounded $tests 'replay* --reporter compact' 0
    $matching = Join-Path $output 'matching.json'
    $divergent = Join-Path $output 'divergent.json'
    if (-not (Test-Path -LiteralPath $matching) -or -not (Test-Path -LiteralPath $divergent)) {
        throw 'Replay fixture export did not complete.'
    }
    Invoke-Bounded $desktop ('--assets "' + $output + '" --replay "' + $matching + '"') 0
    Invoke-Bounded $desktop ('--assets "' + $output + '" --replay "' + $divergent + '"') 2
    Write-Output "Native $Configuration replay command passed matching and divergent checkpoint checks."
} finally {
    $env:DW4_REPLAY_SMOKE_OUTPUT = $originalExport
    if (Test-Path -LiteralPath $output) {
        Remove-Item -LiteralPath $output -Recurse -Force
    }
}
if (Test-Path -LiteralPath $output) {
    throw 'Replay smoke fixture cleanup failed.'
}