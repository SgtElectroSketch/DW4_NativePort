[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$AssetRoot = ''
)

$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if ([string]::IsNullOrEmpty($AssetRoot)) {
    $AssetRoot = Join-Path $root 'native\assets\generated'
}
$AssetRoot = [System.IO.Path]::GetFullPath($AssetRoot)
$output = Join-Path $root ('build\title-parity-' + [Guid]::NewGuid().ToString('N'))
$runner = Join-Path $PSScriptRoot 'run-bounded-process.ps1'
$desktop = Join-Path $root "build\$Configuration\DW4.Desktop.exe"
Add-Type -AssemblyName System.Drawing

function Read-Pixels([string]$Path) {
    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        if ($bitmap.Width -ne 256 -or $bitmap.Height -ne 240) {
            throw 'Title comparison requires 256x240 images.'
        }
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
        } finally {
            $bitmap.UnlockBits($data)
        }
    } finally {
        $bitmap.Dispose()
    }
}

try {
    [System.IO.Directory]::CreateDirectory($output) | Out-Null
    foreach ($frame in 31, 183, 384, 673, 1556) {
        $native = Join-Path $output "native-$frame.png"
        $reference = Join-Path $AssetRoot ('title\reference-{0:D5}.png' -f $frame)
        $arguments = '--assets "' + $AssetRoot + '" --title-check "' + $native + '" --title-frame ' + $frame
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -FilePath $desktop -ArgumentList $arguments `
            -WorkingDirectory $root -TimeoutSeconds 30
        if ($LASTEXITCODE -ne 0) {
            throw "Title render failed: $LASTEXITCODE"
        }
        $expected = Read-Pixels $reference
        $actual = Read-Pixels $native
        if (-not [System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($expected, $actual)) {
            $different = 0
            $first = -1
            for ($index = 0; $index -lt $actual.Length; $index += 4) {
                if ($actual[$index] -ne $expected[$index] -or $actual[$index + 1] -ne $expected[$index + 1] -or
                    $actual[$index + 2] -ne $expected[$index + 2] -or $actual[$index + 3] -ne $expected[$index + 3]) {
                    $different++
                    if ($first -lt 0) { $first = $index / 4 }
                }
            }
            throw "Title frame $frame differs at $different pixels; first pixel $first. Output: $native"
        }
        Write-Output "Title frame $frame matches exactly: 61440 pixels."
    }
    $firstLoop = Join-Path $output 'loop-first.png'
    $secondLoop = Join-Path $output 'loop-second.png'
    foreach ($sample in @(@{ Frame = 1561; Path = $firstLoop }, @{ Frame = 3609; Path = $secondLoop })) {
        $arguments = '--assets "' + $AssetRoot + '" --title-check "' + $sample.Path + '" --title-frame ' + $sample.Frame
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -FilePath $desktop -ArgumentList $arguments `
            -WorkingDirectory $root -TimeoutSeconds 30
        if ($LASTEXITCODE -ne 0) { throw 'Title loop render failed.' }
    }
    if (-not [System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals((Read-Pixels $firstLoop), (Read-Pixels $secondLoop))) {
        throw 'Native title loop did not retain its verified period.'
    }
    Write-Output "Native $Configuration title matched all five reference checkpoints."
} finally {
    if (Test-Path -LiteralPath $output) {
        Remove-Item -LiteralPath $output -Recurse -Force
    }
}