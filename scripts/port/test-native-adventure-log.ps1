[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output = Join-Path $root ('build\adventure-smoke-' + [Guid]::NewGuid().ToString('N'))
$runner = Join-Path $PSScriptRoot 'run-bounded-process.ps1'
$desktop = Join-Path $root "build\$Configuration\DW4.Desktop.exe"
$tests = Join-Path $root "build\$Configuration\DW4.Tests.exe"
$originalExport = $env:DW4_REPLAY_SMOKE_OUTPUT
Add-Type -AssemblyName System.Drawing

function Read-Pixels([string]$Path) {
    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        if ($bitmap.Width -ne 256 -or $bitmap.Height -ne 240) { throw 'Menu comparison requires 256x240 images.' }
        $bounds = [System.Drawing.Rectangle]::new(0, 0, 256, 240)
        $data = $bitmap.LockBits($bounds, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
                                 [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $pixels = [byte[]]::new(256 * 240 * 4)
            for ($row = 0; $row -lt 240; $row++) {
                [System.Runtime.InteropServices.Marshal]::Copy([IntPtr]::Add($data.Scan0, $row * $data.Stride),
                    $pixels, $row * 256 * 4, 256 * 4)
            }
            return ,$pixels
        } finally { $bitmap.UnlockBits($data) }
    } finally { $bitmap.Dispose() }
}

try {
    [System.IO.Directory]::CreateDirectory($output) | Out-Null
    $saveDirectory = Join-Path $output 'saves'
    $image = Join-Path $output 'name-entry.png'
    $arguments = '--log-smoke "' + $image + '" --save-dir "' + $saveDirectory + '"'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -FilePath $desktop -ArgumentList $arguments `
        -WorkingDirectory $root -TimeoutSeconds 30
    if ($LASTEXITCODE -ne 0) { throw "Adventure log smoke failed: $LASTEXITCODE" }
    if (-not (Test-Path -LiteralPath $image) -or -not (Test-Path -LiteralPath (Join-Path $saveDirectory 'adventure-1.json'))) {
        throw 'Adventure smoke did not produce its expected image/save.'
    }
    $referenceDirectory = Join-Path $root 'native\assets\generated\screens\title'
    foreach ($sample in @(@{ Native = 'name-entry-commands.png'; Reference = 'title-02079.png' },
                          @{ Native = 'name-entry-slots.png'; Reference = 'title-02176.png' },
                          @{ Native = 'name-entry.png'; Reference = 'title-02283.png' })) {
        $reference = Join-Path $referenceDirectory $sample.Reference
        if (Test-Path -LiteralPath $reference) {
            $expected = Read-Pixels $reference
            $actual = Read-Pixels (Join-Path $output $sample.Native)
            if (-not [System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($expected, $actual)) {
                $different = 0
                $first = -1
                $tiles = @{}
                for ($index = 0; $index -lt $actual.Length; $index += 4) {
                    if ($actual[$index] -ne $expected[$index] -or $actual[$index + 1] -ne $expected[$index + 1] -or
                        $actual[$index + 2] -ne $expected[$index + 2] -or $actual[$index + 3] -ne $expected[$index + 3]) {
                        $different++
                        if ($first -lt 0) { $first = $index / 4 }
                        $tile = '{0:D2},{1:D2}' -f [int][Math]::Floor((($index / 4) % 256) / 8), [int][Math]::Floor($index / 4 / 256 / 8)
                        if ($tiles.ContainsKey($tile)) { $tiles[$tile]++ } else { $tiles[$tile] = 1 }
                    }
                }
                $summary = ($tiles.Keys | Sort-Object | ForEach-Object { "$_=$($tiles[$_])" }) -join ' '
                throw "Menu $($sample.Native) differs at $different pixels; first pixel $first. Tiles: $summary"
            }
            Write-Output "Menu $($sample.Native) matches the reference: 61440 pixels."
        }
    }
    foreach ($page in 'gender', 'speed', 'confirmation') {
        $null = Read-Pixels (Join-Path $output "name-entry-$page.png")
    }
    if (Test-Path -LiteralPath (Join-Path $saveDirectory 'adventure-2.json')) {
        throw 'Erased adventure slot remains active.'
    }
    $record = Get-Content -LiteralPath (Join-Path $saveDirectory 'adventure-1.json') -Raw | ConvertFrom-Json
    if ($record.name.Count -ne 3 -or $record.version -ne 2 -or $null -eq $record.checksum -or $null -eq $record.world) {
        throw 'Adventure profile fields or integrity marker are missing.'
    }
    $env:DW4_REPLAY_SMOKE_OUTPUT = $output
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -FilePath $tests -ArgumentList 'replay* --reporter compact' `
        -WorkingDirectory $root -TimeoutSeconds 30
    if ($LASTEXITCODE -ne 0) { throw 'Live input fixture generation failed.' }
    $liveSaves = Join-Path $output 'live-saves'
    $script = Join-Path $output 'live-menu.json'
    $liveArguments = '--input-script "' + $script + '" --save-dir "' + $liveSaves + '"'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -FilePath $desktop -ArgumentList $liveArguments `
        -WorkingDirectory $root -TimeoutSeconds 30
    if ($LASTEXITCODE -ne 0) { throw 'Live title/menu input replay failed.' }
    $live = Get-Content -LiteralPath (Join-Path $liveSaves 'adventure-1.json') -Raw | ConvertFrom-Json
    if ($live.name.Count -ne 1 -or $live.name[0] -ne 37) { throw 'Live input did not create the expected adventure name.' }
    Write-Output "Native $Configuration adventure-log command passed create/copy/reopen/erase checks."
} finally {
    $env:DW4_REPLAY_SMOKE_OUTPUT = $originalExport
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Recurse -Force }
}
if (Test-Path -LiteralPath $output) { throw 'Adventure smoke fixture cleanup failed.' }