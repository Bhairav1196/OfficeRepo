[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$fontDirectory = Join-Path $PSScriptRoot '..\TouchGFX\assets\fonts'
$regularDestination = Join-Path $fontDirectory 'Roboto-Regular.ttf'
$mediumDestination = Join-Path $fontDirectory 'Roboto-Medium.ttf'
$downloadUri = 'https://fonts.google.com/download?family=Roboto'

New-Item -ItemType Directory -Force -Path $fontDirectory | Out-Null

function Find-InstalledRoboto([string]$fileName) {
    @(
        (Join-Path $env:WINDIR "Fonts\$fileName"),
        (Join-Path $env:LOCALAPPDATA "Microsoft\Windows\Fonts\$fileName")
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

function Test-TrueTypeFont([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return $false }
    $bytes = [System.IO.File]::ReadAllBytes($path)
    if ($bytes.Length -lt 4096) { return $false }
    $signature = [System.Text.Encoding]::ASCII.GetString($bytes, 0, 4)
    (($bytes[0] -eq 0 -and $bytes[1] -eq 1 -and $bytes[2] -eq 0 -and $bytes[3] -eq 0) -or $signature -eq 'OTTO')
}

$installedRegular = Find-InstalledRoboto 'Roboto-Regular.ttf'
$installedMedium = Find-InstalledRoboto 'Roboto-Medium.ttf'
if ($installedRegular) { Copy-Item -Force -LiteralPath $installedRegular -Destination $regularDestination }
if ($installedMedium) { Copy-Item -Force -LiteralPath $installedMedium -Destination $mediumDestination }

if (-not (Test-TrueTypeFont $regularDestination) -or -not (Test-TrueTypeFont $mediumDestination)) {
    $temporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("swan-roboto-" + [guid]::NewGuid().ToString('N'))
    $archivePath = Join-Path $temporaryDirectory 'Roboto.zip'
    $extractPath = Join-Path $temporaryDirectory 'expanded'
    try {
        New-Item -ItemType Directory -Force -Path $temporaryDirectory | Out-Null
        Write-Host 'Roboto is not installed; downloading the official Google Fonts family...'
        Invoke-WebRequest -UseBasicParsing -Uri $downloadUri -OutFile $archivePath
        Expand-Archive -LiteralPath $archivePath -DestinationPath $extractPath -Force
        $downloadedRegular = Get-ChildItem -LiteralPath $extractPath -Recurse -File -Filter 'Roboto-Regular.ttf' | Select-Object -First 1
        $downloadedMedium = Get-ChildItem -LiteralPath $extractPath -Recurse -File -Filter 'Roboto-Medium.ttf' | Select-Object -First 1
        if (-not $downloadedRegular -or -not $downloadedMedium) {
            throw 'The downloaded family did not contain the required static Regular and Medium TTF files.'
        }
        Copy-Item -Force -LiteralPath $downloadedRegular.FullName -Destination $regularDestination
        Copy-Item -Force -LiteralPath $downloadedMedium.FullName -Destination $mediumDestination
    }
    catch {
        Write-Error ("Roboto preparation failed: {0}`nDownload Roboto from https://fonts.google.com/specimen/Roboto and copy Roboto-Regular.ttf and Roboto-Medium.ttf to:`n  {1}" -f $_.Exception.Message, $fontDirectory)
        exit 1
    }
    finally {
        if (Test-Path -LiteralPath $temporaryDirectory) { Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force }
    }
}

if (-not (Test-TrueTypeFont $regularDestination) -or -not (Test-TrueTypeFont $mediumDestination)) {
    Write-Error 'Roboto files exist but are not valid TrueType/OpenType font binaries.'
    exit 1
}

Write-Host 'Prepared TouchGFX Roboto Regular and Medium source fonts.'
exit 0
