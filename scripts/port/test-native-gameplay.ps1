[CmdletBinding()]
param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Release')

$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output = Join-Path $root ('build\gameplay-smoke-' + [Guid]::NewGuid().ToString('N'))
$runner = Join-Path $PSScriptRoot 'run-bounded-process.ps1'
$desktop = Join-Path $root "build\$Configuration\DW4.Desktop.exe"
Add-Type -AssemblyName System.Drawing
try {
    [System.IO.Directory]::CreateDirectory($output) | Out-Null
    $image = Join-Path $output 'world.png'
    $saves = Join-Path $output 'saves'
    $arguments = '--gameplay-smoke "' + $image + '" --save-dir "' + $saves + '"'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -FilePath $desktop -ArgumentList $arguments `
        -WorkingDirectory $root -TimeoutSeconds 90
    if ($LASTEXITCODE -ne 0) { throw "Native gameplay journey failed: $LASTEXITCODE" }
    foreach ($path in $image, (Join-Path $output 'world-battle.png')) {
        $bitmap = [System.Drawing.Bitmap]::new($path)
        try {
            if ($bitmap.Width -ne 256 -or $bitmap.Height -ne 240) { throw 'Gameplay capture dimensions changed.' }
            $bounds = [System.Drawing.Rectangle]::new(0, 0, 256, 240)
            $data = $bitmap.LockBits($bounds, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
            try {
                $pixels = [byte[]]::new(256 * 240 * 4)
                for ($row = 0; $row -lt 240; $row++) {
                    [System.Runtime.InteropServices.Marshal]::Copy([IntPtr]::Add($data.Scan0, $row * $data.Stride), $pixels, $row * 256 * 4, 256 * 4)
                }
                $lit = 0
                for ($index = 0; $index -lt $pixels.Length; $index += 4) {
                    if ($pixels[$index] -ne 0 -or $pixels[$index + 1] -ne 0 -or $pixels[$index + 2] -ne 0) { $lit++ }
                }
                if ($lit -lt 200) { throw 'Gameplay capture is blank or its referenced assets failed to render.' }
            } finally { $bitmap.UnlockBits($data) }
        } finally { $bitmap.Dispose() }
    }
    $save = Get-Content -LiteralPath (Join-Path $saves 'adventure-1.json') -Raw | ConvertFrom-Json
    if ($save.version -ne 2 -or $save.world.characters.Count -ne 9 -or $save.world.characters[6].level -lt 2 -or
        $save.world.characters[6].experience -lt 12 -or $null -eq $save.checksum) { throw 'Gameplay journey did not persist roster, rewards and progression.' }
    Write-Output "Native $Configuration gameplay passed opening, actor choices/dialogue, castle/town/world, battle rewards, leveling, rendering and reload."
} finally {
    if (Test-Path -LiteralPath $output) { Remove-Item -LiteralPath $output -Recurse -Force }
}
if (Test-Path -LiteralPath $output) { throw 'Gameplay smoke cleanup failed.' }