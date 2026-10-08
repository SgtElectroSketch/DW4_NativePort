param(
    [string]$WorkDirectory = 'work\save-verifier-test'
)

# Regression cases for verify-save-ram.ps1. Each case builds a synthetic battery save (signature from the
# ROM bytes in the generated assembly, one occupied slot with an independently computed checksum), runs the
# verifier against it, and requires the stated outcome. The verifier must reject a save the game would
# reject and must not treat a malformed save as absent evidence.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    if (Test-Path -LiteralPath $WorkDirectory) { Remove-Item -LiteralPath $WorkDirectory -Recurse -Force }
    New-Item -ItemType Directory -Path $WorkDirectory | Out-Null

    # ROM signature bytes at bank $12:$AD71-$AD7D, taken from the "; ADDR BYTES" assembly comments.
    $romBytes = @{}
    foreach ($line in Get-Content -LiteralPath 'src\banks\bank_12.asm') {
        if ($line -match ';\s([0-9A-F]{4})((?:\s[0-9A-F]{2})+)\s') {
            $address = [Convert]::ToInt32($matches[1], 16)
            foreach ($value in $matches[2].Trim() -split '\s+') {
                $romBytes[$address] = [Convert]::ToInt32($value, 16)
                $address++
            }
        }
    }
    $signature = @(0..12 | ForEach-Object { $romBytes[0xAD71 + $_] })

    function New-Save {
        [byte[]]$sram = New-Object byte[] 0x2000
        for ($slot = 0; $slot -lt 3; $slot++) {
            $record = 0x02EF + ($slot * 752)
            for ($index = 0; $index -lt 752; $index++) { $sram[$record + $index] = 0x4B }
        }
        # Slot 1: a deterministic payload and its CRC-16 (polynomial $1021, seed $3A3A, most significant bit first).
        $state = 0x3A3A
        for ($index = 0; $index -lt 750; $index++) {
            $value = (($index * 7) + 3) -band 0xFF
            $sram[0x02EF + 2 + $index] = [byte]$value
            for ($bit = 0; $bit -lt 8; $bit++) {
                $top = (($state -shr 15) -bxor ($value -shr 7)) -band 1
                $state = ($state -shl 1) -band 0xFFFF
                $value = ($value -shl 1) -band 0xFF
                if ($top -eq 1) { $state = $state -bxor 0x1021 }
            }
        }
        $sram[0x02EF] = [byte]($state -band 0xFF)
        $sram[0x02F0] = [byte]($state -shr 8)
        for ($index = 0; $index -lt 13; $index++) {
            $sram[0x0BBF + $index] = [byte]$signature[$index]
            $sram[0x0BCC + $index] = [byte]$signature[$index]
        }
        return ,$sram
    }

    function Invoke-Case([string]$name, [bool]$expectPass, [string]$expectText, [scriptblock]$build, [string]$configPath) {
        $directory = Join-Path $WorkDirectory ($name -replace '[^A-Za-z0-9]', '-')
        New-Item -ItemType Directory -Path $directory | Out-Null
        $entries = & $build
        $zip = [System.IO.Compression.ZipFile]::Open((Join-Path $directory 'state1.zip'), 'Create')
        try {
            foreach ($entry in $entries) {
                $stream = $zip.CreateEntry($entry.Name).Open()
                try { $stream.Write($entry.Bytes, 0, $entry.Bytes.Length) } finally { $stream.Dispose() }
            }
        }
        finally { $zip.Dispose() }
        $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', 'scripts\verify-save-ram.ps1',
            '-StateDirectory', $directory, '-ReportPath', (Join-Path $directory 'report.md'))
        if ($configPath) { $arguments += @('-ConfigPath', $configPath) }
        $previous = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        $output = (& powershell.exe @arguments 2>&1 | Out-String)
        $passed = $LASTEXITCODE -eq 0
        $ErrorActionPreference = $previous
        $ok = ($passed -eq $expectPass) -and (-not $expectText -or $output.Contains($expectText))
        $outcome = if ($passed) { 'accepted' } else { 'rejected' }
        if ($ok) { Write-Output "PASS $name" } else { Write-Output "FAIL ${name}: verifier $outcome the case; output: $($output.Trim())" }
        return $ok
    }

    $results = @()
    $results += Invoke-Case 'valid save is accepted and counted' $true '1/1 occupied' {
        @(@{ Name = 'state1/game.sav'; Bytes = (New-Save) })
    }
    $results += Invoke-Case 'signature intact in one copy is accepted' $true '1/1 occupied' {
        $sram = New-Save
        $sram[0x0BBF + 4] = 0x00
        @(@{ Name = 'state1/game.sav'; Bytes = $sram })
    }
    $results += Invoke-Case 'signature corrupted in both copies is rejected' $false 'signature' {
        $sram = New-Save
        $sram[0x0BBF + 4] = 0x00
        $sram[0x0BCC + 4] = 0x00
        @(@{ Name = 'state1/game.sav'; Bytes = $sram })
    }
    $results += Invoke-Case 'stored checksum that differs is rejected' $false 'checksum' {
        $sram = New-Save
        $sram[0x02EF] = [byte](($sram[0x02EF] + 1) -band 0xFF)
        @(@{ Name = 'state1/game.sav'; Bytes = $sram })
    }
    $results += Invoke-Case 'save of the wrong length is rejected' $false '8191' {
        $sram = New-Save
        [byte[]]$short = $sram[0..0x1FFE]
        @(@{ Name = 'state1/game.sav'; Bytes = $short })
    }
    $results += Invoke-Case 'archive without a battery save is reported as absent' $true 'no archived battery saves' {
        @(@{ Name = 'state1/notes.txt'; Bytes = [System.Text.Encoding]::ASCII.GetBytes('no save here') })
    }
    $shortLedger = Join-Path $WorkDirectory 'save-ram-short-signature.tsv'
    (Get-Content -LiteralPath 'config\save-ram.tsv') -replace "^AdventureLogSignature`t6BBF`t6BCC", "AdventureLogSignature`t6BBF`t6BCB" |
        Set-Content -LiteralPath $shortLedger -Encoding ASCII
    $results += Invoke-Case 'signature ledger span shorter than the compared bytes is rejected' $false 'AdventureLogSignature' {
        @(@{ Name = 'state1/game.sav'; Bytes = (New-Save) })
    } $shortLedger

    $failed = @($results | Where-Object { $_ -is [bool] -and -not $_ }).Count
    $total = @($results | Where-Object { $_ -is [bool] }).Count
    $results | Where-Object { $_ -isnot [bool] }
    Write-Output "save verifier test: $($total - $failed)/$total cases passed"
    if ($failed -ne 0) { exit 1 }
}
finally {
    Pop-Location
}
