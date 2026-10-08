[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output = Join-Path $root ('build\graphics-smoke-' + [Guid]::NewGuid().ToString('N'))
$runner = Join-Path $PSScriptRoot 'run-bounded-process.ps1'
$desktop = Join-Path $root "build\$Configuration\DW4.Desktop.exe"
$encoding = [System.Text.UTF8Encoding]::new($false)
Add-Type -AssemblyName System.Drawing
$catalogs = [ordered]@{
    'text/dialogue.json' = '{"schema_version":1,"messages":[]}'
    'maps/index.json' = '{"schema_version":1,"locations":[]}'
    'sprites/field/index.json' = '{"schema_version":1,"sets":[]}'
    'monsters/index.json' = '{"schema_version":1,"monsters":[]}'
    'graphics/index.json' = '{"schema_version":1,"chr":[],"fonts":[]}'
    'screens/index.json' = '{"schema_version":1,"scenes":[]}'
    'audio/index.json' = '{"schema_version":1,"music":[],"sfx":[]}'
}

try {
    foreach ($relativePath in $catalogs.Keys) {
        $path = Join-Path $output $relativePath
        [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($path)) | Out-Null
        [System.IO.File]::WriteAllText($path, $catalogs[$relativePath], $encoding)
    }
    $hash = $null
    foreach ($iteration in 1..2) {
        $image = Join-Path $output "render-$iteration.png"
        $nativeArguments = '--assets "' + $output + '" --render-check "' + $image + '"'
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -FilePath $desktop -ArgumentList $nativeArguments `
            -WorkingDirectory $root -TimeoutSeconds 30
        if ($LASTEXITCODE -ne 0) {
            throw "Render smoke process failed: $LASTEXITCODE"
        }
        $decoded = [System.Drawing.Image]::FromFile($image)
        try {
            if ($decoded.RawFormat.Guid -ne [System.Drawing.Imaging.ImageFormat]::Png.Guid -or
                $decoded.Width -ne 256 -or $decoded.Height -ne 240) {
                throw 'Render command did not produce a 256x240 PNG.'
            }
        } finally {
            $decoded.Dispose()
        }
        $currentHash = (Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash
        if ($null -ne $hash -and $hash -ne $currentHash) {
            throw 'Repeated native render commands produced different PNGs.'
        }
        $hash = $currentHash
    }
    Write-Output "Native $Configuration graphics command passed 256x240 output and repeated-render checks."
} finally {
    if (Test-Path -LiteralPath $output) {
        Remove-Item -LiteralPath $output -Recurse -Force
    }
}
if (Test-Path -LiteralPath $output) {
    throw 'Graphics smoke cleanup failed.'
}