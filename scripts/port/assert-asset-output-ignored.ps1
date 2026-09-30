param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath($RepositoryRoot).TrimEnd('\', '/')
$output = [System.IO.Path]::GetFullPath($OutputPath).TrimEnd('\', '/')
$rootPrefix = $root + [System.IO.Path]::DirectorySeparatorChar

if (-not $output.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    Write-Host "asset output is outside the repository: $output"
    exit 0
}

$relative = $output.Substring($rootPrefix.Length).Replace('\', '/')
$probe = "$relative/.dw4-copyright-probe"
& git -C $root check-ignore --no-index --quiet -- $probe
if ($LASTEXITCODE -ne 0) {
    throw "asset output is inside the repository but is not Git-ignored: $output"
}

Write-Host "asset output copyright boundary verified: $relative/"