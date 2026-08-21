param(
    [Parameter(Mandatory = $true)]
    [string]$GltfPath
)

$ErrorActionPreference = 'Stop'

$gltf = Get-Content -Raw -LiteralPath $GltfPath | ConvertFrom-Json
if ($gltf.buffers.Count -ne 1) {
    throw "Expected one flattened buffer, found $($gltf.buffers.Count)."
}

$binPath = Join-Path (Split-Path -Parent $GltfPath) $gltf.buffers[0].uri
$bytes = [System.Collections.Generic.List[byte]]::new()
$sourceBytes = [System.IO.File]::ReadAllBytes($binPath)
$bytes.AddRange($sourceBytes)

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backupDir = Join-Path (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $GltfPath))) "VoxelConversion\before-normal-repair-$stamp"
New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
Copy-Item -LiteralPath $GltfPath -Destination (Join-Path $backupDir (Split-Path -Leaf $GltfPath))
Copy-Item -LiteralPath $binPath -Destination (Join-Path $backupDir (Split-Path -Leaf $binPath))

$targetPattern = '^(CRS[123]A|GIB[0-9][AB]|SGC[12][AB]|SGW[12][AB]|SQSHA|ICEN[ABCD])$'
$repaired = 0

function Convert-Signed10([uint32]$value) {
    $v = [int]($value -band 0x3ff)
    if ($v -ge 512) { $v -= 1024 }
    return [single]($v / 512.0)
}

for ($nodeIndex = 0; $nodeIndex -lt $gltf.nodes.Count; $nodeIndex++) {
    $node = $gltf.nodes[$nodeIndex]
    if (($node.name -notmatch $targetPattern) -or ($null -eq $node.mesh)) { continue }

    $mesh = $gltf.meshes[[int]$node.mesh]
    foreach ($primitive in $mesh.primitives) {
        if ($null -ne $primitive.attributes.NORMAL) { continue }

        $positionAccessor = $gltf.accessors[[int]$primitive.attributes.POSITION]
        $positionView = $gltf.bufferViews[[int]$positionAccessor.bufferView]
        $sourceBase = [int64]$positionView.byteOffset + [int64]$positionAccessor.byteOffset
        $stride = if ($positionView.byteStride) { [int]$positionView.byteStride } else { 44 }
        $count = [int]$positionAccessor.count

        while (($bytes.Count % 4) -ne 0) { $bytes.Add(0) }
        $normalOffset = $bytes.Count

        for ($i = 0; $i -lt $count; $i++) {
            $packedOffset = $sourceBase + ($i * $stride) + 20
            $packed = [BitConverter]::ToUInt32($sourceBytes, [int]$packedOffset)
            $nx = Convert-Signed10 $packed
            $ny = Convert-Signed10 ($packed -shr 10)
            $nz = Convert-Signed10 ($packed -shr 20)
            $bytes.AddRange([BitConverter]::GetBytes($nx))
            $bytes.AddRange([BitConverter]::GetBytes($ny))
            $bytes.AddRange([BitConverter]::GetBytes($nz))
        }

        $normalLength = $count * 12
        $newView = [pscustomobject]@{
            buffer = 0
            byteOffset = $normalOffset
            byteLength = $normalLength
            byteStride = 12
        }
        $viewIndex = $gltf.bufferViews.Count
        $gltf.bufferViews += $newView

        $newAccessor = [pscustomobject]@{
            bufferView = $viewIndex
            componentType = 5126
            type = 'VEC3'
            count = $count
        }
        $accessorIndex = $gltf.accessors.Count
        $gltf.accessors += $newAccessor
        $primitive.attributes | Add-Member -NotePropertyName NORMAL -NotePropertyValue $accessorIndex
        $repaired++
    }
}

# Restore the authored stock barrel replacement and detach the converted copies.
foreach ($node in $gltf.nodes) {
    if ($node.name -eq 'BAR1A_STOCK_DISABLED') { $node.name = 'BAR1A' }
    elseif ($node.name -eq 'BAR1B_STOCK_DISABLED') { $node.name = 'BAR1B' }
}

$root = $gltf.nodes | Where-Object name -eq 'rtgl1_main_root' | Select-Object -First 1
if (-not $root) { throw 'rtgl1_main_root was not found.' }
$root.children = @($root.children | Where-Object { [int]$_ -notin @(188, 189) })
$gltf.nodes[188].name = 'BAR1A_CONVERTED_UNUSED'
$gltf.nodes[189].name = 'BAR1B_CONVERTED_UNUSED'

$gltf.buffers[0].byteLength = $bytes.Count
[System.IO.File]::WriteAllBytes($binPath, $bytes.ToArray())
$json = $gltf | ConvertTo-Json -Depth 100
[System.IO.File]::WriteAllText($GltfPath, $json, [System.Text.UTF8Encoding]::new($false))

Write-Output "Repaired NORMAL attributes: $repaired"
Write-Output "Restored stock BAR1A/B and detached converted barrel nodes."
Write-Output "Backup: $backupDir"
Write-Output "Buffer bytes: $($bytes.Count)"
