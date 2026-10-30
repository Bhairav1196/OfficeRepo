param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot
)

$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath $ProjectRoot).Path
$files = @(
    'TouchGFX/STM32U5G9_DK2_W5500_TouchGFX_ModbusTCP_Master.touchgfx',
    'TouchGFX/assets/texts/texts.xml',
    'TouchGFX/gui/include/gui/screen1_screen/Screen1View.hpp',
    'TouchGFX/gui/src/screen1_screen/Screen1View.cpp'
)

foreach ($relative in $files) {
    if (-not (Test-Path -LiteralPath (Join-Path $project $relative) -PathType Leaf)) {
        throw "Not the expected STM32U5G9_W5500 project; missing: $relative"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $relative) -PathType Leaf)) {
        throw "Patch is incomplete; missing: $relative"
    }
}

$backup = Join-Path $project ('AO_PATCH_BACKUP_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
foreach ($relative in $files) {
    $saved = Join-Path $backup $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $saved) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $project $relative) -Destination $saved
}

foreach ($relative in $files) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $relative) -Destination (Join-Path $project $relative) -Force
}

$view = Get-Content -LiteralPath (Join-Path $project $files[3]) -Raw
if (-not $view.Contains('void Screen1View::toggleAoStep()') -or
    -not $view.Contains('aoFineStep ? 1 : 10') -or
    $view.Contains('showControl = false')) {
    throw "AO patch verification failed. Your original four files are in $backup"
}

Write-Host 'AO patch applied: four matching TouchGFX source/design files.'
Write-Host "Original files backed up at: $backup"
Write-Host 'Now open the .touchgfx file in TouchGFX Designer and select Run Simulator.'
