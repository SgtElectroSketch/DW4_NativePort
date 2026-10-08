[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$prepare = Join-Path $PSScriptRoot 'prepare-native-asset-catalogs.ps1'
$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('dw4-native-catalog-tests-' + [Guid]::NewGuid().ToString('N'))
$encoding = [System.Text.UTF8Encoding]::new($false)
$checks = 0
$fixtures = [ordered]@{
    'text/dialogue.json' = '{"messages":[{"id":0,"group":0,"index":0,"symbols":[1,2,255]}]}'
    'maps/index.json' = '{"locations":[{"map":0}],"extra":{"value":7}}'
    'sprites/field/index.json' = '{"sets":[{"set":0}]}'
    'monsters/index.json' = '{"monsters":[{"monster_id":0}]}'
    'graphics/index.json' = '{"chr":[{"tiles":1}],"fonts":[{}]}'
    'screens/index.json' = '{"scenes":[{"scene":"authored-example"}]}'
    'audio/index.json' = '{"music":[{"track":1,"seconds":1.25}],"sfx":[{"sfx":1}]}'
}

function Assert-Condition([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        throw $Message
    }
    $script:checks += 1
}

function Write-Catalog([string]$RelativePath, [string]$Json) {
    $path = Join-Path $temporaryRoot $RelativePath
    [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($path)) | Out-Null
    [System.IO.File]::WriteAllText($path, $Json, $encoding)
}

function Reset-Catalogs {
    foreach ($relativePath in $fixtures.Keys) {
        Write-Catalog $relativePath $fixtures[$relativePath]
    }
}

function Assert-Rejected([scriptblock]$Operation, [string]$ExpectedMessage) {
    $rejected = $false
    try {
        & $Operation | Out-Null
    } catch {
        if (-not $_.Exception.Message.Contains($ExpectedMessage)) {
            throw
        }
        $rejected = $true
    }
    Assert-Condition $rejected "Expected rejection containing: $ExpectedMessage"
}

try {
    Reset-Catalogs
    & $prepare -OutputPath $temporaryRoot -RepositoryRoot $repositoryRoot | Out-Null
    $hashes = @{}
    foreach ($relativePath in $fixtures.Keys) {
        $path = Join-Path $temporaryRoot $relativePath
        $document = Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
        Assert-Condition ($document.schema_version -eq 1) "Missing version: $relativePath"
        $document.PSObject.Properties.Remove('schema_version')
        $before = $fixtures[$relativePath] | ConvertFrom-Json | ConvertTo-Json -Depth 100 -Compress
        $after = $document | ConvertTo-Json -Depth 100 -Compress
        Assert-Condition ($before -ceq $after) "Preparation changed existing fields: $relativePath"
        $hashes[$relativePath] = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    }
    & $prepare -OutputPath $temporaryRoot -RepositoryRoot $repositoryRoot | Out-Null
    foreach ($relativePath in $fixtures.Keys) {
        $hash = (Get-FileHash -LiteralPath (Join-Path $temporaryRoot $relativePath) -Algorithm SHA256).Hash
        Assert-Condition ($hash -eq $hashes[$relativePath]) "Preparation is not idempotent: $relativePath"
    }

    $invalidCases = @(
        @{ Path = 'text/dialogue.json'; Json = '[{"messages":[]}]'; Message = 'root must be an object' },
        @{ Path = 'maps/index.json'; Json = '{"locations":[false]}'; Message = 'must contain objects' },
        @{ Path = 'screens/index.json'; Json = '{"scenes":null}'; Message = 'must be an array' },
        @{ Path = 'audio/index.json'; Json = '{"schema_version":2,"music":[],"sfx":[]}'; Message = 'schema_version' },
        @{ Path = 'audio/index.json'; Json = '{"schema_version":1.0,"music":[],"sfx":[]}'; Message = 'schema_version' },
        @{ Path = 'audio/index.json'; Json = '{"music":private-payload}'; Message = 'Invalid catalog JSON' }
    )
    foreach ($invalidCase in $invalidCases) {
        Reset-Catalogs
        Write-Catalog $invalidCase.Path $invalidCase.Json
        $untouchedPath = Join-Path $temporaryRoot 'graphics/index.json'
        $beforeHash = (Get-FileHash -LiteralPath $untouchedPath -Algorithm SHA256).Hash
        Assert-Rejected { & $prepare -OutputPath $temporaryRoot -RepositoryRoot $repositoryRoot } $invalidCase.Message
        $afterHash = (Get-FileHash -LiteralPath $untouchedPath -Algorithm SHA256).Hash
        Assert-Condition ($beforeHash -eq $afterHash) 'Validation failure modified another catalog.'
    }

    Reset-Catalogs
    foreach ($relativePath in $fixtures.Keys) {
        if ($relativePath -ne 'text/dialogue.json') {
            Remove-Item -LiteralPath (Join-Path $temporaryRoot $relativePath)
        }
    }
    & $prepare -OutputPath $temporaryRoot -RepositoryRoot $repositoryRoot | Out-Null
    $partial = Get-Content -LiteralPath (Join-Path $temporaryRoot 'text/dialogue.json') -Raw | ConvertFrom-Json
    Assert-Condition ($partial.schema_version -eq 1) 'Partial extraction was not prepared.'
    Assert-Rejected { & $prepare -OutputPath $repositoryRoot -RepositoryRoot $repositoryRoot -ValidateOutputOnly } 'repository root'
    Assert-Rejected { & $prepare -OutputPath (Join-Path $repositoryRoot 'unignored-native-assets-test') -RepositoryRoot $repositoryRoot -ValidateOutputOnly } 'not Git-ignored'
    $temporaryFiles = @(Get-ChildItem -LiteralPath $temporaryRoot -Recurse -File -Filter '*.tmp')
    Assert-Condition ($temporaryFiles.Count -eq 0) 'Preparation left temporary files behind.'
    Write-Output "Native asset preparation passed $checks assertions."
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
if (Test-Path -LiteralPath $temporaryRoot) {
    throw 'Test fixture cleanup failed.'
}