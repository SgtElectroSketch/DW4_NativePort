param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot
)

$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath($RepositoryRoot)
$forbiddenExtensions = @(
    '.nes', '.rom', '.fds', '.unf', '.unif',
    '.ips', '.bps', '.ups', '.xdelta',
    '.png', '.jpg', '.jpeg', '.gif', '.bmp', '.webp',
    '.wav', '.vgm', '.ogg', '.mp3', '.flac', '.mid', '.midi',
    '.sav', '.srm', '.zip', '.7z', '.rar'
)
$generatedAssetPath = '(?i)(^|/)(assets?|resources?)/(generated|extracted|runtime|packaged?|export(?:ed)?)(/|$)'
$knownExtractedCatalog = '(?i)(^|/)(text/(dialogue|names|credits|ui_strings)\.(json|tsv|txt)|maps/(index\.json|locations/.+\.json|world/.+\.json|tilesets/.+\.json)|sprites/field/(index\.json|sets/.+\.json)|monsters/(index\.json|monsters\.tsv|unnamed-records\.json|.+/info\.(json|txt)|presentation-tiles/index\.json)|graphics/index\.json|screens/(index\.json|.+/index\.json)|audio/(index\.json|(music|sfx)/.+\.json))$'
$documentationImage = '(?i)^docs/DevNotes/Images/.+\.(png|jpe?g|gif|bmp|webp)$'

$paths = @(& git -C $root ls-files --cached --others --exclude-standard)
if ($LASTEXITCODE -ne 0) {
    throw 'git ls-files failed while checking the copyright boundary'
}

$violations = [System.Collections.Generic.List[string]]::new()
foreach ($path in $paths) {
    $normalized = $path.Replace('\', '/')
    $extension = [System.IO.Path]::GetExtension($normalized).ToLowerInvariant()
    $forbiddenMedia = $extension -in $forbiddenExtensions -and $normalized -notmatch $documentationImage
    if ($forbiddenMedia -or
        $normalized -match $generatedAssetPath -or
        $normalized -match $knownExtractedCatalog) {
        $violations.Add($normalized)
    }
}

if ($violations.Count -ne 0) {
    $details = ($violations | Sort-Object -Unique | ForEach-Object { "  $_" }) -join [Environment]::NewLine
    throw "copyright boundary violation: ROM or extracted asset payloads are visible to Git:$([Environment]::NewLine)$details"
}

Write-Host "copyright boundary verified: no ROM or extracted asset payloads are visible to Git"