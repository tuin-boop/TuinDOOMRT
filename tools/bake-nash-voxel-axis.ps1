param(
    [string]$GameDir = 'C:\doomrttest600\Game'
)

$ErrorActionPreference = 'Stop'
$gltfPath = Join-Path $GameDir 'rt\replace\set_0_misc.gltf'
$binPath = Join-Path $GameDir 'rt\replace\set_0_misc.bin'
if (!(Test-Path -LiteralPath $gltfPath) -or !(Test-Path -LiteralPath $binPath)) {
    throw "Converted RT model files were not found below $GameDir"
}

$json = Get-Content -LiteralPath $gltfPath -Raw | ConvertFrom-Json
if ($json.asset.extras.tuindoomNashAxisBaked) {
    Write-Host 'Nash voxel axis conversion is already baked; nothing changed.'
    exit 0
}

$namePattern = '^(CRS[123]A|GIB[0-9][AB]|SGC[12][AB]|SGW[12][AB]|SQSHA|ICEN[ABCD])$'
$targetNodes = @($json.nodes | Where-Object { $_.name -match $namePattern })
if ($targetNodes.Count -ne 36) {
    throw "Expected 36 Nash voxel nodes, found $($targetNodes.Count)."
}

$positionAccessors = [Collections.Generic.HashSet[int]]::new()
$normalAccessors = [Collections.Generic.HashSet[int]]::new()
foreach ($node in $targetNodes) {
    $primitive = $json.meshes[[int]$node.mesh].primitives[0]
    [void]$positionAccessors.Add([int]$primitive.attributes.POSITION)
    if ($null -ne $primitive.attributes.NORMAL) {
        [void]$normalAccessors.Add([int]$primitive.attributes.NORMAL)
    }
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backupDir = Join-Path $GameDir "VoxelConversion\before-nash-axis-$stamp"
New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
Copy-Item -LiteralPath $gltfPath -Destination (Join-Path $backupDir 'set_0_misc.gltf')
Copy-Item -LiteralPath $binPath -Destination (Join-Path $backupDir 'set_0_misc.bin')

$bytes = [IO.File]::ReadAllBytes($binPath)
function Transform-Accessor([int]$accessorIndex, [bool]$updateBounds) {
    $accessor = $json.accessors[$accessorIndex]
    if ([int]$accessor.componentType -ne 5126 -or $accessor.type -ne 'VEC3') {
        throw "Accessor $accessorIndex is not a float VEC3."
    }
    $view = $json.bufferViews[[int]$accessor.bufferView]
    $base = [int]$view.byteOffset + [int]$accessor.byteOffset
    $stride = if ($view.byteStride) { [int]$view.byteStride } else { 12 }
    $mins = @([double]::PositiveInfinity, [double]::PositiveInfinity, [double]::PositiveInfinity)
    $maxs = @([double]::NegativeInfinity, [double]::NegativeInfinity, [double]::NegativeInfinity)
    for ($i = 0; $i -lt [int]$accessor.count; $i++) {
        $offset = $base + ($i * $stride)
        $x = [BitConverter]::ToSingle($bytes, $offset)
        $y = [BitConverter]::ToSingle($bytes, $offset + 4)
        $z = [BitConverter]::ToSingle($bytes, $offset + 8)
        # Voxel Doom uses Y-up; the RT replacement renderer uses Z-up.
        # Rotate +90 degrees around X: (x, y, z) -> (x, -z, y).
        $out = @([single]$x, [single](-$z), [single]$y)
        for ($axis = 0; $axis -lt 3; $axis++) {
            [BitConverter]::GetBytes($out[$axis]).CopyTo($bytes, $offset + (4 * $axis))
            if ($updateBounds) {
                $mins[$axis] = [Math]::Min($mins[$axis], $out[$axis])
                $maxs[$axis] = [Math]::Max($maxs[$axis], $out[$axis])
            }
        }
    }
    if ($updateBounds) {
        $accessor.min = @($mins)
        $accessor.max = @($maxs)
    }
}

foreach ($index in $positionAccessors) { Transform-Accessor $index $true }
foreach ($index in $normalAccessors) { Transform-Accessor $index $false }

if ($null -eq $json.asset.extras) {
    $json.asset | Add-Member -NotePropertyName extras -NotePropertyValue ([pscustomobject]@{})
}
$json.asset.extras | Add-Member -NotePropertyName tuindoomNashAxisBaked -NotePropertyValue $true -Force
[IO.File]::WriteAllBytes($binPath, $bytes)
$json | ConvertTo-Json -Depth 100 -Compress | Set-Content -LiteralPath $gltfPath -Encoding UTF8

Write-Host "Rotated $($positionAccessors.Count) POSITION and $($normalAccessors.Count) NORMAL accessors."
Write-Host "Backup: $backupDir"

