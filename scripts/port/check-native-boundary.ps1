param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot
)

$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath($RepositoryRoot)
$nativeRoot = [System.IO.Path]::GetFullPath((Join-Path $root 'native'))
$testsRoot = [System.IO.Path]::GetFullPath((Join-Path $root 'tests'))
$allowedRoots = @($nativeRoot, $testsRoot)
$projects = @(Get-ChildItem -LiteralPath $nativeRoot -Filter '*.vcxproj' -File -Recurse)
if (Test-Path -LiteralPath $testsRoot -PathType Container) {
    $projects += Get-ChildItem -LiteralPath $testsRoot -Filter '*.vcxproj' -File -Recurse
}

foreach ($project in $projects) {
    [xml]$document = Get-Content -LiteralPath $project.FullName -Raw
    $assemblyItems = $document.SelectNodes("//*[local-name()='MASM' or local-name()='NASM']")
    if ($assemblyItems.Count -ne 0) {
        throw "native project contains an assembly build item: $($project.FullName)"
    }

    foreach ($item in $document.SelectNodes("//*[@Include]")) {
        $include = [string]$item.Include
        if ($include -match '(?i)\.(?:asm|s|65s)$') {
            throw "native project includes assembly input '$($item.Include)': $($project.FullName)"
        }
    }

    foreach ($reference in $document.SelectNodes("//*[local-name()='ProjectReference']")) {
        $target = [System.IO.Path]::GetFullPath((Join-Path $project.DirectoryName ([string]$reference.Include)))
        $allowed = $false
        foreach ($allowedRoot in $allowedRoots) {
            if ($target.StartsWith($allowedRoot + [System.IO.Path]::DirectorySeparatorChar,
                    [System.StringComparison]::OrdinalIgnoreCase)) {
                $allowed = $true
                break
            }
        }
        if (-not $allowed) {
            throw "native project reference escapes the native/test boundary: $target"
        }
    }
}

$solutionPath = Join-Path $root 'DragonWarrior4.Native.sln'
if ((Get-Content -LiteralPath $solutionPath -Raw) -match '(?i)reference[\\/]') {
    throw 'DragonWarrior4.Native.sln must not include reference projects'
}

Write-Host "native boundary verified: $($projects.Count) project(s), no assembly inputs or reference dependencies"