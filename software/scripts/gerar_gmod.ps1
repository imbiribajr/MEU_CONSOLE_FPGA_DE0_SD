param(
    [string]$BuiltinPath,
    [string]$ElfPath,

    [Parameter(Mandatory = $true)]
    [string]$Title,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [string]$ToolchainBin = "F:\altera\13.0sp1\nios2eds\bin\gnu\H-i686-mingw32\bin"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($BuiltinPath) -eq [string]::IsNullOrWhiteSpace($ElfPath)) {
    throw "Informe exatamente um entre -BuiltinPath ou -ElfPath."
}

$outFull = [System.IO.Path]::GetFullPath($OutputPath)
$outDir = Split-Path -Parent $outFull
if ($outDir -and !(Test-Path $outDir)) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}

if (![string]::IsNullOrWhiteSpace($BuiltinPath)) {
    $content = "$BuiltinPath`r`n$Title`r`n"
    [System.IO.File]::WriteAllText($outFull, $content, [System.Text.Encoding]::ASCII)
    Write-Host "GMOD texto gerado:" $outFull
    exit 0
}

$readelf = Join-Path $ToolchainBin "nios2-elf-readelf.exe"
$nm = Join-Path $ToolchainBin "nios2-elf-nm.exe"

if (!(Test-Path $ElfPath)) {
    throw "ELF nao encontrado: $ElfPath"
}

$elfFull = (Resolve-Path $ElfPath).Path
$readelfText = & $readelf -l $elfFull
if ($LASTEXITCODE -ne 0) {
    throw "readelf falhou para $elfFull"
}
$readelfHeaderText = & $readelf -h $elfFull
if ($LASTEXITCODE -ne 0) {
    throw "readelf -h falhou para $elfFull"
}
$nmText = & $nm $elfFull
if ($LASTEXITCODE -ne 0) {
    throw "nm falhou para $elfFull"
}

$segments = @()
foreach ($line in $readelfText) {
    if ($line -match "^\s*LOAD\s+0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)\s+([RWE ]+)") {
        $phys = [Convert]::ToUInt32($matches[3], 16)
        $segments += [pscustomobject]@{
            Offset   = [Convert]::ToUInt32($matches[1], 16)
            VirtAddr = [Convert]::ToUInt32($matches[2], 16)
            PhysAddr = $phys
            FileSiz  = [Convert]::ToUInt32($matches[4], 16)
            MemSiz   = [Convert]::ToUInt32($matches[5], 16)
            Flags    = $matches[6].Trim()
        }
    }
}

$entryAddr = 0
foreach ($line in $readelfHeaderText) {
    if ($line -match "Entry point address:\s+0x([0-9A-Fa-f]+)") {
        $entryAddr = [Convert]::ToUInt32($matches[1], 16)
        break
    }
}
if ($entryAddr -eq 0) {
    throw "Entry point nao encontrado em $elfFull"
}

$allowedSegments = $segments | Where-Object {
    ($_.PhysAddr -ge 0x00800000 -and $_.PhysAddr -lt 0x01000000) -or
    ($_.PhysAddr -ge 0x0180C000 -and $_.PhysAddr -lt 0x01810000)
}
if ($allowedSegments.Count -eq 0) {
    throw "Nenhum LOAD segment permitido encontrado em $elfFull"
}

$stackAddr = 0
$gpAddr = 0
foreach ($line in $nmText) {
    if (($stackAddr -eq 0) -and ($line -match "^\s*([0-9A-Fa-f]+)\s+\w\s+__alt_stack_pointer$")) {
        $stackAddr = [Convert]::ToUInt32($matches[1], 16)
    }
    if (($gpAddr -eq 0) -and ($line -match "^\s*([0-9A-Fa-f]+)\s+\w\s+_gp$")) {
        $gpAddr = [Convert]::ToUInt32($matches[1], 16)
    }
}
if (($stackAddr -eq 0) -or ($gpAddr -eq 0)) {
    throw "Simbolos de stack/gp nao encontrados em $elfFull"
}

[byte[]]$elfBytes = [System.IO.File]::ReadAllBytes($elfFull)
$contentSegments = @($allowedSegments | Where-Object { $_.FileSiz -ne 0 } | Sort-Object PhysAddr)
if ($contentSegments.Count -eq 0) {
    throw "Segmentos sem conteudo em $elfFull"
}
if ($contentSegments.Count -gt 8) {
    throw "Muitos segmentos ($($contentSegments.Count)) para o formato GIMG/GMOD"
}

[uint32]$headerSize = 64 + ($contentSegments.Count * 16)
[uint32]$payloadOffset = $headerSize
$segmentEntries = @()
$payloadChunks = New-Object System.Collections.Generic.List[byte[]]

foreach ($seg in $contentSegments) {
    [byte[]]$chunk = New-Object byte[] $seg.FileSiz
    [Array]::Copy($elfBytes, [int]$seg.Offset, $chunk, 0, [int]$seg.FileSiz)
    $payloadChunks.Add($chunk)
    $segmentEntries += [pscustomobject]@{
        DestAddr   = $seg.VirtAddr
        DataOffset = [uint32]$payloadOffset
        FileSize   = $seg.FileSiz
        MemSize    = $seg.MemSiz
    }
    $payloadOffset += $seg.FileSiz
}

[byte[]]$titleBytes = New-Object byte[] 32
[Text.Encoding]::ASCII.GetBytes($Title.ToUpperInvariant(), 0, [Math]::Min($Title.Length, 31), $titleBytes, 0) | Out-Null

$ms = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($ms)
$bw.Write([UInt32]0x47494D47)
$bw.Write([UInt32]2)
$bw.Write([UInt32]$segmentEntries.Count)
$bw.Write([UInt32]$entryAddr)
$bw.Write([UInt32]$stackAddr)
$bw.Write([UInt32]$gpAddr)
$bw.Write([UInt32]$headerSize)
$bw.Write([UInt32]0)
$bw.Write($titleBytes)

foreach ($seg in $segmentEntries) {
    $bw.Write([UInt32]$seg.DestAddr)
    $bw.Write([UInt32]$seg.DataOffset)
    $bw.Write([UInt32]$seg.FileSize)
    $bw.Write([UInt32]$seg.MemSize)
}

foreach ($chunk in $payloadChunks) {
    $bw.Write($chunk)
}

$bw.Flush()
[System.IO.File]::WriteAllBytes($outFull, $ms.ToArray())
Write-Host "GMOD binario gerado:" $outFull
