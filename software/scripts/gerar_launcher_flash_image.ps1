param(
    [string]$LauncherElf,
    [string]$OutputImage,
    [string]$OutputHeader,
    [string]$ToolchainBin = "F:\altera\13.0sp1\nios2eds\bin\gnu\H-i686-mingw32\bin"
)

$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))

if ([string]::IsNullOrWhiteSpace($LauncherElf)) {
    $LauncherElf = Join-Path $ProjectRoot "software\games\launcher\app\main.elf"
}
if ([string]::IsNullOrWhiteSpace($OutputImage)) {
    $OutputImage = Join-Path $ProjectRoot "software\games\launcher\app\launcher_flash.img"
}
if ([string]::IsNullOrWhiteSpace($OutputHeader)) {
    $OutputHeader = Join-Path $ProjectRoot "software\games\flashwrite\app\launcher_flash_payload.h"
}

$nm = Join-Path $ToolchainBin "nios2-elf-nm.exe"
$readelf = Join-Path $ToolchainBin "nios2-elf-readelf.exe"

if (!(Test-Path $LauncherElf)) {
    throw "Launcher ELF nao encontrado: $LauncherElf"
}

$nmOut = & $nm -n $LauncherElf
if ($LASTEXITCODE -ne 0) {
    throw "nm falhou"
}
$readelfOut = & $readelf -l $LauncherElf
if ($LASTEXITCODE -ne 0) {
    throw "readelf falhou"
}

function Get-SymbolValue([string[]]$Lines, [string]$SymbolName) {
    foreach ($line in $Lines) {
        if ($line -match ("^([0-9A-Fa-f]+)\s+\w\s+" + [Regex]::Escape($SymbolName) + "$")) {
            return [Convert]::ToUInt32($matches[1], 16)
        }
    }
    throw "Simbolo nao encontrado: $SymbolName"
}

$entry = Get-SymbolValue $nmOut "_start"
$gp = Get-SymbolValue $nmOut "_gp"
$sp = Get-SymbolValue $nmOut "__alt_stack_pointer"

$segments = @()
foreach ($line in $readelfOut) {
    if ($line -match "^\s*LOAD\s+0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)\s+([RWE ]+)") {
        $segments += [pscustomobject]@{
            Offset   = [Convert]::ToUInt32($matches[1], 16)
            VirtAddr = [Convert]::ToUInt32($matches[2], 16)
            PhysAddr = [Convert]::ToUInt32($matches[3], 16)
            FileSiz  = [Convert]::ToUInt32($matches[4], 16)
            MemSiz   = [Convert]::ToUInt32($matches[5], 16)
            Flags    = $matches[6].Trim()
        }
    }
}

$sdramSegments = @($segments | Where-Object {
    $_.PhysAddr -ge 0x00800000 -and $_.PhysAddr -lt 0x01000000
} | Sort-Object PhysAddr)
if ($sdramSegments.Count -eq 0) {
    throw "Nenhum segmento LOAD em SDRAM encontrado no launcher ELF."
}

$loadAddr = ($sdramSegments | Select-Object -First 1).PhysAddr
$maxEnd = 0
foreach ($seg in $sdramSegments) {
    $segEnd = $seg.PhysAddr + $seg.MemSiz
    if ($segEnd -gt $maxEnd) {
        $maxEnd = $segEnd
    }
}

[byte[]]$payload = New-Object byte[] ($maxEnd - $loadAddr)
[byte[]]$elfBytes = [System.IO.File]::ReadAllBytes($LauncherElf)

foreach ($seg in $sdramSegments) {
    if ($seg.FileSiz -ne 0) {
        [Array]::Copy(
            $elfBytes,
            [int]$seg.Offset,
            $payload,
            [int]($seg.PhysAddr - $loadAddr),
            [int]$seg.FileSiz
        )
    }
}

$checksum = 0
foreach ($b in $payload) {
    $checksum = ($checksum + $b) -band 0xFFFFFFFF
}

$magic = 0x4C4E4348
$version = 1

$ms = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($ms)
$bw.Write([UInt32]$magic)
$bw.Write([UInt32]$version)
$bw.Write([UInt32]$loadAddr)
$bw.Write([UInt32]$entry)
$bw.Write([UInt32]$gp)
$bw.Write([UInt32]$sp)
$bw.Write([UInt32]$payload.Length)
$bw.Write([UInt32]$checksum)
$bw.Write($payload)
$bw.Flush()

[System.IO.File]::WriteAllBytes($OutputImage, $ms.ToArray())
Write-Host "Imagem gerada:" $OutputImage
Write-Host ("entry=0x{0:X8} gp=0x{1:X8} sp=0x{2:X8} size={3}" -f $entry, $gp, $sp, $payload.Length)

$img = [System.IO.File]::ReadAllBytes($OutputImage)
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("#ifndef LAUNCHER_FLASH_PAYLOAD_H")
$lines.Add("#define LAUNCHER_FLASH_PAYLOAD_H")
$lines.Add("")
$lines.Add("#include <stdint.h>")
$lines.Add("")
$lines.Add(("static const uint32_t launcher_flash_payload_size = {0}u;" -f $img.Length))
$lines.Add("static const uint8_t launcher_flash_payload[] = {")

for ($i = 0; $i -lt $img.Length; $i += 12) {
    $chunk = $img[$i..([Math]::Min($i + 11, $img.Length - 1))]
    $hex = ($chunk | ForEach-Object { ("0x{0:X2}" -f $_) }) -join ", "
    if ($i + 12 -lt $img.Length) {
        $lines.Add("    $hex,")
    } else {
        $lines.Add("    $hex")
    }
}

$lines.Add("};")
$lines.Add("")
$lines.Add("#endif")

$headerDir = Split-Path -Parent $OutputHeader
if (!(Test-Path $headerDir)) {
    New-Item -ItemType Directory -Force $headerDir | Out-Null
}
[System.IO.File]::WriteAllLines($OutputHeader, $lines)
Write-Host "Header gerado:" $OutputHeader
