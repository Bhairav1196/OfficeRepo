param(
    [string]$ProjectRoot = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $ProjectRoot = (Resolve-Path $ProjectRoot).Path
}

$RequiredTouchGfxVersion = 42601
$ProjectTouchGfx = Join-Path $ProjectRoot "Middlewares\ST\touchgfx"
$ImageRel = "framework\tools\imageconvert\build\win\imageconvert.out"
$FontRel  = "framework\tools\fontconvert\build\win\fontconvert.out"
$ImageDst = Join-Path $ProjectTouchGfx $ImageRel
$FontDst  = Join-Path $ProjectTouchGfx $FontRel

function Get-TouchGfxVersion([string]$TouchGfxRoot) {
    $vf = Join-Path $TouchGfxRoot "framework\include\touchgfx\Version.hpp"
    if (-not (Test-Path -LiteralPath $vf -PathType Leaf)) { return $null }
    $m = Select-String -LiteralPath $vf -Pattern '^\s*#define\s+TOUCHGFX_VERSION\s+(\d+)\s*$' | Select-Object -First 1
    if ($null -eq $m) { return $null }
    return [int]$m.Matches[0].Groups[1].Value
}

function Resolve-ToolRoot([string]$Root) {
    if ([string]::IsNullOrWhiteSpace($Root)) { return $null }
    foreach ($shape in @($Root, (Join-Path $Root "touchgfx"), (Join-Path $Root "Middlewares\ST\touchgfx"))) {
        if (-not (Test-Path -LiteralPath $shape -PathType Container)) { continue }
        $rp = (Resolve-Path -LiteralPath $shape).Path
        if ((Get-TouchGfxVersion $rp) -ne $RequiredTouchGfxVersion) { continue }
        if ((Test-Path -LiteralPath (Join-Path $rp $ImageRel) -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $rp $FontRel) -PathType Leaf)) {
            return $rp
        }
    }
    return $null
}

if ((Get-TouchGfxVersion $ProjectTouchGfx) -ne $RequiredTouchGfxVersion) {
    throw "Project middleware is not TouchGFX 4.26.1 (TOUCHGFX_VERSION=42601)."
}

if ((Test-Path -LiteralPath $ImageDst -PathType Leaf) -and (Test-Path -LiteralPath $FontDst -PathType Leaf)) {
    Write-Host "TouchGFX 4.26.1 asset converters are already present."
    exit 0
}

Write-Host "Locating official TouchGFX 4.26.1 tools using deterministic paths only..."
Write-Host "No recursive filesystem scan will be performed."

$candidates = @()
foreach ($v in @($env:TOUCHGFX_TOOL_SOURCE, $env:TOUCHGFX_ROOT, $env:TOUCHGFX_PATH)) {
    if (-not [string]::IsNullOrWhiteSpace($v)) { $candidates += $v }
}
$candidates += "C:\TouchGFX\4.26.1\touchgfx"
$candidates += "C:\TouchGFX\4.26.1"
if ($env:USERPROFILE) {
    $candidates += (Join-Path $env:USERPROFILE "TouchGFX\4.26.1\touchgfx")
    $candidates += (Join-Path $env:USERPROFILE "STM32Cube\Repository\X-CUBE-TOUCHGFX\4.26.1\Middlewares\ST\touchgfx")
    $candidates += (Join-Path $env:USERPROFILE "STM32Cube\Repository\Packs\STMicroelectronics\X-CUBE-TOUCHGFX\4.26.1\Middlewares\ST\touchgfx")
}

$sourceTouchGfx = $null
foreach ($candidate in $candidates | Select-Object -Unique) {
    $sourceTouchGfx = Resolve-ToolRoot $candidate
    if ($sourceTouchGfx) { break }
}

# Resolve the installed TouchGFX CLI and walk only its parent chain (bounded).
if (-not $sourceTouchGfx) {
    try {
        $cmd = Get-Command touchgfx -ErrorAction Stop
        $dir = Split-Path -Parent $cmd.Source
        for ($i = 0; $i -lt 7 -and $dir; $i++) {
            $sourceTouchGfx = Resolve-ToolRoot $dir
            if ($sourceTouchGfx) { break }
            $parent = Split-Path -Parent $dir
            if ([string]::IsNullOrWhiteSpace($parent) -or $parent -eq $dir) { break }
            $dir = $parent
        }
    } catch { }
}

if (-not $sourceTouchGfx) {
    Write-Host ""
    Write-Host "ERROR: TouchGFX 4.26.1 converter tools were not found in the known locations." -ForegroundColor Red
    Write-Host "Set the official TouchGFX root explicitly and rerun this script:"
    Write-Host "  set TOUCHGFX_TOOL_SOURCE=C:\TouchGFX\4.26.1\touchgfx"
    Write-Host "  Tools\PREPARE_TOUCHGFX_ENVIRONMENT.bat"
    Write-Host ""
    Write-Host "Required files below that root:"
    Write-Host "  framework\tools\imageconvert\build\win\imageconvert.out"
    Write-Host "  framework\tools\fontconvert\build\win\fontconvert.out"
    exit 2
}

Write-Host "Using official TouchGFX tool source: $sourceTouchGfx"
$srcImageDir = Split-Path -Parent (Join-Path $sourceTouchGfx $ImageRel)
$srcFontDir  = Split-Path -Parent (Join-Path $sourceTouchGfx $FontRel)
$dstImageDir = Split-Path -Parent $ImageDst
$dstFontDir  = Split-Path -Parent $FontDst
New-Item -ItemType Directory -Force -Path $dstImageDir | Out-Null
New-Item -ItemType Directory -Force -Path $dstFontDir | Out-Null
Copy-Item -Path (Join-Path $srcImageDir '*') -Destination $dstImageDir -Force -Recurse
Copy-Item -Path (Join-Path $srcFontDir '*')  -Destination $dstFontDir  -Force -Recurse
if (-not (Test-Path -LiteralPath $ImageDst -PathType Leaf)) { throw "imageconvert.out copy failed" }
if (-not (Test-Path -LiteralPath $FontDst -PathType Leaf))  { throw "fontconvert.out copy failed" }
Write-Host "TouchGFX 4.26.1 converter tools prepared successfully."
exit 0
