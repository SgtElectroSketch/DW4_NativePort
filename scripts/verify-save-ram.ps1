param(
    [string]$ConfigPath = 'config\save-ram.tsv',
    [string]$ConstantsPath = 'src\constants\ram.inc',
    [string]$WritePath = 'analysis\fceux-writes.tsv',
    [string]$ReportPath = 'analysis\save-ram-report.md',
    [string]$BankDirectory = 'src\banks',
    [string]$StateDirectory = 'States'
)

$ErrorActionPreference = 'Stop'

function Convert-Hex([string]$value) {
    return [Convert]::ToInt32($value, 16)
}

# Every generated assembly line ends with "; ADDR BYTES", so the exact ROM bytes are recoverable from the
# verified source without a second copy of the ROM.
function Read-BankBytes([string]$bank) {
    $bytes = @{}
    foreach ($line in Get-Content -LiteralPath (Join-Path $BankDirectory "bank_$bank.asm")) {
        if ($line -match ';\s([0-9A-F]{4})((?:\s[0-9A-F]{2})+)\s') {
            $address = Convert-Hex $matches[1]
            foreach ($value in $matches[2].Trim() -split '\s+') {
                $bytes[$address] = Convert-Hex $value
                $address++
            }
        }
    }
    return $bytes
}

function Get-Bytes($bank, [int]$address, [int]$count) {
    return @(for ($index = 0; $index -lt $count; $index++) {
        if (-not $bank.ContainsKey($address + $index)) { throw ('generated assembly has no byte at ${0:X4}' -f ($address + $index)) }
        $bank[$address + $index]
    })
}

function Assert-Bytes($bank, [int]$address, [int[]]$expected, [string]$meaning) {
    $actual = Get-Bytes $bank $address $expected.Count
    if (($actual -join ',') -ne ($expected -join ',')) {
        throw ('{0}: expected {1} at ${2:X4}, found {3}' -f $meaning, (($expected | ForEach-Object { '{0:X2}' -f $_ }) -join ' '), $address, (($actual | ForEach-Object { '{0:X2}' -f $_ }) -join ' '))
    }
}

function Get-Word($bank, [int]$address) {
    $pair = Get-Bytes $bank $address 2
    return $pair[0] -bor ($pair[1] -shl 8)
}

# Bank $1F:$C8AD folds one byte into the 16-bit state $12/$13, most significant bit first.
function Get-SlotChecksum([byte[]]$data, [int]$offset, [int]$count, [int]$seed, [int]$polynomial) {
    $state = $seed
    for ($index = 0; $index -lt $count; $index++) {
        $value = [int]$data[$offset + $index]
        for ($bit = 0; $bit -lt 8; $bit++) {
            $feedback = (($state -shr 8) -bxor $value) -band 0x80
            $state = ($state -shl 1) -band 0xFFFF
            $value = ($value -shl 1) -band 0xFF
            if ($feedback -ne 0) { $state = $state -bxor $polynomial }
        }
    }
    return $state
}

$constants = @{}
foreach ($line in Get-Content -LiteralPath $ConstantsPath) {
    if ($line -match '^\s*(?<name>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*\$(?<address>[0-9A-F]{4})') {
        $constants[$matches.name] = Convert-Hex $matches.address
    }
}

$writes = @{}
foreach ($line in Get-Content -LiteralPath $WritePath) {
    if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith('#')) { continue }
    $writes[(Convert-Hex (($line -split "`t")[0]))] = $true
}

$ranges = @()
foreach ($line in Get-Content -LiteralPath $ConfigPath) {
    if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith('#')) { continue }
    $columns = $line -split "`t"
    if ($columns.Count -ne 6) { throw "invalid save RAM row: $line" }
    $start = Convert-Hex $columns[1]
    $endExclusive = Convert-Hex $columns[2]
    if ($start -lt 0x6000 -or $endExclusive -gt 0x8000 -or $start -ge $endExclusive) {
        throw "save RAM range is outside cartridge SRAM: $line"
    }
    if ($constants.ContainsKey($columns[0]) -and $constants[$columns[0]] -ne $start) {
        throw "$($columns[0]) does not match ram.inc"
    }
    $ranges += [pscustomobject]@{ Name = $columns[0]; Start = $start; EndExclusive = $endExclusive; Initialization = $columns[3]; Persistence = $columns[4]; Evidence = $columns[5] }
}

for ($index = 1; $index -lt $ranges.Count; $index++) {
    if ($ranges[$index].Start -lt $ranges[$index - 1].EndExclusive) {
        throw "overlapping save RAM ranges: $($ranges[$index - 1].Name) and $($ranges[$index].Name)"
    }
}

# Adventure Log layout and checksum parameters are read from the ROM bytes the save code itself loads.
$bank12 = Read-BankBytes '12'
$bank1F = Read-BankBytes '1F'
$payloadLength = Get-Word $bank12 0xAD7E
$recordLength = Get-Word $bank12 0xAD80
$slotBase = Get-Word $bank12 0xAD88
$workBase = Get-Word $bank12 0xAD8A
Assert-Bytes $bank12 0xACA8 @(0xA9, 0x3A, 0x85, 0x12, 0x85, 0x13) 'checksum seed LDA #imm / STA $12 / STA $13'
$seedByte = (Get-Bytes $bank12 0xACA9 1)[0]
$seed = $seedByte -bor ($seedByte -shl 8)
Assert-Bytes $bank12 0xACB4 @(0x20, 0xAD, 0xC8) 'checksum loop JSR $C8AD'
Assert-Bytes $bank1F 0xC8AD @(0xA0, 0x08, 0xA5, 0x13, 0x45, 0x16, 0x06, 0x12, 0x26, 0x13, 0x06, 0x16, 0x0A, 0x90, 0x0C, 0xA5, 0x12, 0x49) 'CRC step prefix'
Assert-Bytes $bank1F 0xC8C0 @(0x85, 0x12, 0xA5, 0x13, 0x49) 'CRC step high-byte EOR'
Assert-Bytes $bank1F 0xC8C6 @(0x85, 0x13, 0x88, 0xD0, 0xE4, 0x60) 'CRC step loop and return'
$polynomial = (Get-Bytes $bank1F 0xC8BF 1)[0] -bor ((Get-Bytes $bank1F 0xC8C5 1)[0] -shl 8)
# SEC / LDY #$00 / LDA $12 / SBC ($00),Y / INY / LDA $13 / SBC ($00),Y / BNE: only the high-byte result is tested.
Assert-Bytes $bank12 0xACC8 @(0x38, 0xA0, 0x00, 0xA5, 0x12, 0xF1, 0x00, 0xC8, 0xA5, 0x13, 0xF1, 0x00, 0xD0, 0x06) 'checksum validator subtraction'

# Signature test at $AC73-$AC83: LDX #len-1 / LDA rom,X / CMP copy1,X / BEQ / CMP copy2,X / BNE fail / DEX / BPL.
Assert-Bytes $bank12 0xAC73 @(0xA2) 'signature loop LDX #imm'
Assert-Bytes $bank12 0xAC75 @(0xBD) 'signature loop LDA abs,X'
Assert-Bytes $bank12 0xAC78 @(0xDD) 'signature loop CMP abs,X against the first copy'
Assert-Bytes $bank12 0xAC7B @(0xF0, 0x05, 0xDD) 'signature loop BEQ / CMP abs,X against the second copy'
Assert-Bytes $bank12 0xAC80 @(0xD0, 0x08, 0xCA, 0x10, 0xF0) 'signature loop BNE / DEX / BPL'
$signatureLength = (Get-Bytes $bank12 0xAC74 1)[0] + 1
$signatureRom = Get-Word $bank12 0xAC76
$signatureCopies = @((Get-Word $bank12 0xAC79), (Get-Word $bank12 0xAC7E))
$signature = Get-Bytes $bank12 $signatureRom $signatureLength
$signatureFields = @('AdventureLogSignature', 'AdventureLogSignatureCopy')
for ($copy = 0; $copy -lt 2; $copy++) {
    $range = $ranges | Where-Object Name -eq $signatureFields[$copy]
    if ($null -eq $range -or $range.Start -ne $signatureCopies[$copy] -or $range.EndExclusive -ne $signatureCopies[$copy] + $signatureLength) {
        throw ('{0} must cover ${1:X4}-${2:X4}, the {3} bytes bank $12:$AC73-$AC83 compares' -f $signatureFields[$copy], $signatureCopies[$copy], ($signatureCopies[$copy] + $signatureLength - 1), $signatureLength)
    }
}

# Slot indices are accepted below CMP #count at $AAF6, and the slot list is built from LDA #count-1 at $AA60.
Assert-Bytes $bank12 0xAAF6 @(0xC9) 'slot bound CMP #imm'
Assert-Bytes $bank12 0xAA60 @(0xA9) 'slot list LDA #imm'
$slotCount = (Get-Bytes $bank12 0xAAF7 1)[0]
if ((Get-Bytes $bank12 0xAA61 1)[0] -ne $slotCount - 1) { throw 'slot bound at $AAF6 disagrees with the slot list start at $AA60' }
if ($slotBase + ($slotCount * $recordLength) -ne $signatureCopies[0]) { throw 'Adventure Log slots do not end where the signature begins' }

if ($recordLength -ne $payloadLength + 2) { throw "Adventure Log record length $recordLength is not payload length $payloadLength plus the checksum word" }
if ($workBase + $payloadLength -ne $slotBase) { throw 'working save span does not end where the Adventure Log slots begin' }
for ($slot = 0; $slot -lt $slotCount; $slot++) {
    $name = "AdventureLogSlot$($slot + 1)"
    $range = $ranges | Where-Object Name -eq $name
    $expectedStart = $slotBase + ($slot * $recordLength)
    if ($null -eq $range -or $range.Start -ne $expectedStart -or $range.EndExclusive -ne $expectedStart + $recordLength) {
        throw ('{0} must cover ${1:X4}-${2:X4} per bank $12:$AD80/$AD88' -f $name, $expectedStart, ($expectedStart + $recordLength - 1))
    }
}

# Archived battery saves are local evidence (the archives are not distributed). A save that is present
# must be one the game itself would accept: 8,192 bytes, a signature that passes the test at $AC73, and in
# every occupied slot exactly the checksum the recovered algorithm computes. A malformed save is an error,
# never absent evidence.
$archives = @()
if (Test-Path -LiteralPath $StateDirectory) {
    $archives = @(Get-ChildItem -LiteralPath $StateDirectory -Filter '*.zip' | Sort-Object Name)
}
$saveCount = 0
$occupiedSlots = 0
$erasedSlots = 0
$mismatches = @()
$archivesWithoutSave = 0
if ($archives.Count -gt 0) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    foreach ($archive in $archives) {
        $zip = [System.IO.Compression.ZipFile]::OpenRead($archive.FullName)
        try {
            $saves = @($zip.Entries | Where-Object { $_.FullName -like '*.sav' })
            if ($saves.Count -eq 0) { $archivesWithoutSave++ }
            foreach ($entry in $saves) {
                if ($entry.Length -ne 0x2000) {
                    throw "battery save $($archive.Name):$($entry.FullName) is $($entry.Length) bytes; the cartridge SRAM is 8192 bytes"
                }
                $buffer = New-Object System.IO.MemoryStream
                $stream = $entry.Open()
                try { $stream.CopyTo($buffer) } finally { $stream.Dispose() }
                [byte[]]$sram = $buffer.ToArray()
                $saveCount++
                for ($index = 0; $index -lt $signatureLength; $index++) {
                    if ($sram[$signatureCopies[0] - 0x6000 + $index] -ne $signature[$index] -and
                        $sram[$signatureCopies[1] - 0x6000 + $index] -ne $signature[$index]) {
                        $mismatches += ('{0}: signature byte {1} matches the ROM in neither copy, so the game would erase every slot' -f $archive.Name, $index)
                        break
                    }
                }
                for ($slot = 0; $slot -lt $slotCount; $slot++) {
                    $record = $slotBase - 0x6000 + ($slot * $recordLength)
                    if (@($sram[$record..($record + 4)] | Where-Object { $_ -ne 0x4B }).Count -eq 0) {
                        $erasedSlots++
                        continue
                    }
                    $occupiedSlots++
                    $stored = [int]$sram[$record] -bor ([int]$sram[$record + 1] -shl 8)
                    $computed = Get-SlotChecksum $sram ($record + 2) $payloadLength $seed $polynomial
                    if ($computed -ne $stored) {
                        $mismatches += ('{0} slot {1}: stored checksum ${2:X4}, computed ${3:X4}' -f $archive.Name, ($slot + 1), $stored, $computed)
                    }
                }
            }
        }
        finally {
            $zip.Dispose()
        }
    }
    if ($mismatches.Count -gt 0) {
        throw "archived battery saves contradict the recovered Adventure Log rules: $($mismatches -join '; ')"
    }
}
$archiveSummary = if ($saveCount -gt 0) {
    "$saveCount/$saveCount archived battery saves pass the signature test and $occupiedSlots/$occupiedSlots occupied slots store exactly the computed checksum; $erasedSlots slots are erased"
} else {
    'no archived battery saves are present in this checkout, so the checksum and signature rules were not corroborated against saved data in this run'
}
if ($archivesWithoutSave -gt 0) { $archiveSummary += "; $archivesWithoutSave archives contain no battery save" }

$report = @(
    '# Save RAM Verification',
    '',
    'The MMC1 cartridge exposes battery-backed SRAM at `$6000-$7FFF`. Runtime tracing observed direct writes to every listed address. The game plays out of a working save at `$6001-$62EE`; recovered code reads and writes those addresses in place.',
    '',
    '## Adventure Log slots and checksum',
    '',
    ('The working save is not the saved game. Bank `$12` keeps {0} Adventure Log records of {1} bytes starting at `${2:X4}` (record address = `${2:X4}` + slot * {1}, bank `$12:$AD1C-$AD31`). Each record is a two-byte checksum followed by a {3}-byte copy of the working save `${4:X4}-${5:X4}`.' -f $slotCount, $recordLength, $slotBase, $payloadLength, $workBase, ($workBase + $payloadLength - 1)),
    '',
    ('- **Checksum** (`$12:$AC9B`): CRC-16, polynomial `${0:X4}`, most significant bit first, state seeded with `${1:X4}`, over the {2} payload bytes at record offsets 2-{3}. The per-byte update is `$1F:$C8AD`, the same routine that advances the random-number state in `$12/$13`, so computing a checksum overwrites that state.' -f $polynomial, $seed, $payloadLength, ($recordLength - 1)),
    '- **Write** (`$12:$ACDF`): stores the checksum little-endian in record bytes 0-1. The save service `$12:$AC53` (bank `$12` directory entry `$0E`) copies the working save into the slot and then calls it.',
    '- **Validate** (`$12:$ACBD`): a record whose bytes 0-4 are all `$4B` is erased (`A=$01`). Otherwise the routine subtracts the stored word from the recomputed one and branches on the high-byte result only; the low-byte result is discarded and only its borrow carries over. A slot is therefore accepted (`A=$00`) when computed minus stored lies in `$0000-$00FF`, not only on exact equality, and rejected (`A=$80`) otherwise. `$12:$AA5B` erases a rejected slot.',
    ('- **Signature**: `$12:$AC73` compares the {0} ROM bytes `$12:${1:X4}-${2:X4}` with the copies at `${3:X4}` and `${4:X4}`; each byte must match in at least one copy. When one does not, `$12:$AA41` rewrites both copies and fills every slot with `$4B`. The verifier applies the same test to each archived save and requires both ledger fields to span exactly those bytes.' -f $signatureLength, $signatureRom, ($signatureRom + $signatureLength - 1), $signatureCopies[0], $signatureCopies[1]),
    '',
    "Corroboration: $archiveSummary.",
    '',
    'Limits: observed writes show that each byte is written, not that a slot is intact; slot integrity rests on the checksum above. Load-time behavior beyond the checksum and signature tests, and every working-save byte outside the fields below, remain undocumented here.',
    '',
    '## Fields',
    '',
    '| Field | Range | Runtime writes | Initialization | Evidence |',
    '|---|---:|---:|---|---|'
)
$missing = @()
foreach ($range in $ranges) {
    $hitCount = @($writes.Keys | Where-Object { $_ -ge $range.Start -and $_ -lt $range.EndExclusive }).Count
    if ($hitCount -ne ($range.EndExclusive - $range.Start)) { $missing += $range.Name }
    $report += ('| {0} | `${1:X4}-${2:X4}` | {3}/{4} | {5} | {6} |' -f $range.Name, $range.Start, ($range.EndExclusive - 1), $hitCount, ($range.EndExclusive - $range.Start), $range.Initialization, $range.Evidence)
}
$report | Set-Content -LiteralPath $ReportPath -Encoding UTF8

if ($missing.Count -gt 0) {
    throw "save RAM fields lack complete runtime write evidence: $($missing -join ', ')"
}
Write-Output ('Save RAM verification passed: {0} fields; Adventure Log checksum CRC-16 polynomial ${1:X4} seed ${2:X4} over {3} bytes; {4}' -f $ranges.Count, $polynomial, $seed, $payloadLength, $archiveSummary)
