[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$OutputPath,
    [string]$RepositoryRoot = '',
    [switch]$ValidateOutputOnly
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrEmpty($RepositoryRoot)) {
    $RepositoryRoot = Join-Path $PSScriptRoot '..\..'
}
$root = [System.IO.Path]::GetFullPath($RepositoryRoot).TrimEnd('\', '/')
$output = [System.IO.Path]::GetFullPath($OutputPath).TrimEnd('\', '/')
if ([string]::Equals($root, $output, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Asset output cannot be the repository root.'
}
& (Join-Path $PSScriptRoot 'assert-asset-output-ignored.ps1') -RepositoryRoot $root -OutputPath $output
if ($ValidateOutputOnly) {
    exit 0
}

$catalogs = [ordered]@{
    'text/dialogue.json' = @('messages')
    'maps/index.json' = @('locations')
    'sprites/field/index.json' = @('sets')
    'monsters/index.json' = @('monsters')
    'graphics/index.json' = @('chr', 'fonts')
    'screens/index.json' = @('scenes')
    'audio/index.json' = @('music', 'sfx')
}
$updates = [System.Collections.Generic.List[object]]::new()
foreach ($relativePath in $catalogs.Keys) {
    $path = Join-Path $output $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        continue
    }
    if ((Get-Item -LiteralPath $path).Length -gt 32MB) {
        throw "Catalog exceeds the 32 MiB preparation limit: $relativePath"
    }
    try {
        $document = Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
    } catch {
        throw "Invalid catalog JSON: $relativePath. Run extract-native-assets.cmd again."
    }
    if ($document -isnot [System.Management.Automation.PSCustomObject]) {
        throw "Catalog root must be an object: $relativePath"
    }
    $version = $document.PSObject.Properties['schema_version']
    if ($null -ne $version -and
        (($version.Value -isnot [int] -and $version.Value -isnot [long]) -or $version.Value -ne 1)) {
        throw "Unsupported existing schema_version: $relativePath. Run a complete extraction again."
    }
    foreach ($key in $catalogs[$relativePath]) {
        $collection = $document.PSObject.Properties[$key]
        if ($null -eq $collection -or $collection.Value -isnot [array]) {
            throw "Catalog field must be an array: $relativePath : $key"
        }
        foreach ($record in $collection.Value) {
            if ($record -isnot [System.Management.Automation.PSCustomObject]) {
                throw "Catalog collection must contain objects: $relativePath : $key"
            }
        }
    }
    $document | Add-Member -MemberType NoteProperty -Name 'schema_version' -Value 1 -Force
    $updates.Add([pscustomobject]@{
        Path = $path
        Json = ($document | ConvertTo-Json -Depth 100) + "`n"
    })
}
if ($updates.Count -eq 0) {
    throw 'No generated catalogs found. Run extract-native-assets.cmd first.'
}

$encoding = [System.Text.UTF8Encoding]::new($false)
foreach ($update in $updates) {
    $temporaryPath = $update.Path + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    try {
        [System.IO.File]::WriteAllText($temporaryPath, $update.Json, $encoding)
        [System.IO.File]::Replace($temporaryPath, $update.Path, [NullString]::Value)
    } finally {
        if (Test-Path -LiteralPath $temporaryPath) {
            Remove-Item -LiteralPath $temporaryPath -Force
        }
    }
}
Write-Output "Prepared schema version 1 for $($updates.Count) generated catalog(s)."