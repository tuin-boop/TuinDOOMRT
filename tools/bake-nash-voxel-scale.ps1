param(
    [Parameter(Mandatory = $true)]
    [string]$GltfPath,

    [double]$Scale = (1.0 / 32.0)
)

$ErrorActionPreference = 'Stop'

$gltf = Get-Content -Raw -LiteralPath $GltfPath | ConvertFrom-Json
if ($gltf.buffers.Count -ne 1) {
    throw "Expected one flattened GLTF buffer, found $($gltf.buffers.Count)."
}

$binPath = Join-Path (Split-Path -Parent $GltfPath) $gltf.buffers[0].uri
$bytes = [System.IO.File]::ReadAllBytes($binPath)
$targetPattern = '^(CRS[123]A|GIB[0-9][AB]|SGC[12][AB]|SGW[12][AB]|SQSHA|ICEN[ABCD])$'
$targets = @($gltf.nodes | Where-Object { $_.name -match $targetPattern })

if ($targets.Count -ne 36) {
    throw "Expected 36 Nash voxel nodes, found $($targets.Count)."
}

$positionAccessors = [System.Collections.Generic.HashSet[int]]::new()
foreach ($node in $targets) {
    if ($null -eq $node.mesh) { throw "Target node '$($node.name)' has no mesh." }
    $mesh = $gltf.meshes[[int]$node.mesh]
    foreach ($primitive in $mesh.primitives) {
        [void]$positionAccessors.Add([int]$primitive.attributes.POSITION)
    }
}

# Do not alter any accessor shared with a non-Nash model.
for ($nodeIndex = 0; $nodeIndex -lt $gltf.nodes.Count; $nodeIndex++) {
    $node = $gltf.nodes[$nodeIndex]
    if (($node.name -match $targetPattern) -or ($null -eq $node.mesh)) { continue }
    foreach ($primitive in $gltf.meshes[[int]$node.mesh].primitives) {
        if ($positionAccessors.Contains([int]$primitive.attributes.POSITION)) {
            throw "POSITION accessor $($primitive.attributes.POSITION) is shared by Nash node(s) and non-Nash node '$($node.name)'."
        }
    }
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$rtRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $GltfPath))
$backupDir = Join-Path $rtRoot "VoxelConversion\before-nash-baked-scale-$stamp"
New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
Copy-Item -LiteralPath $GltfPath -Destination (Join-Path $backupDir (Split-Path -Leaf $GltfPath))
Copy-Item -LiteralPath $binPath -Destination (Join-Path $backupDir (Split-Path -Leaf $binPath))

foreach ($accessorIndex in $positionAccessors) {
    $accessor = $gltf.accessors[$accessorIndex]
    if (($accessor.componentType -ne 5126) -or ($accessor.type -ne 'VEC3')) {
        throw "POSITION accessor $accessorIndex is not a float VEC3."
    }

    $view = $gltf.bufferViews[[int]$accessor.bufferView]
    $viewOffset = if ($null -ne $view.byteOffset) { [int64]$view.byteOffset } else { 0 }
    $accessorOffset = if ($null -ne $accessor.byteOffset) { [int64]$accessor.byteOffset } else { 0 }
    $base = $viewOffset + $accessorOffset
    $stride = if ($null -ne $view.byteStride) { [int]$view.byteStride } else { 12 }

    $minimum = @([double]::PositiveInfinity, [double]::PositiveInfinity, [double]::PositiveInfinity)
    $maximum = @([double]::NegativeInfinity, [double]::NegativeInfinity, [double]::NegativeInfinity)

    for ($vertex = 0; $vertex -lt [int]$accessor.count; $vertex++) {
        $vertexOffset = [int]($base + ($vertex * $stride))
        for ($axis = 0; $axis -lt 3; $axis++) {
            $offset = $vertexOffset + ($axis * 4)
            $value = [single]([BitConverter]::ToSingle($bytes, $offset) * $Scale)
            [BitConverter]::GetBytes($value).CopyTo($bytes, $offset)
            if ($value -lt $minimum[$axis]) { $minimum[$axis] = $value }
            if ($value -gt $maximum[$axis]) { $maximum[$axis] = $value }
        }
    }

    $newMin = @([single]$minimum[0], [single]$minimum[1], [single]$minimum[2])
    $newMax = @([single]$maximum[0], [single]$maximum[1], [single]$maximum[2])
    if ($accessor.PSObject.Properties.Name -contains 'min') {
        $accessor.min = $newMin
    } else {
        $accessor | Add-Member -NotePropertyName min -NotePropertyValue $newMin
    }
    if ($accessor.PSObject.Properties.Name -contains 'max') {
        $accessor.max = $newMax
    } else {
        $accessor | Add-Member -NotePropertyName max -NotePropertyValue $newMax
    }
}

foreach ($node in $targets) {
    $node.scale = @(1.0, 1.0, 1.0)
}

[System.IO.File]::WriteAllBytes($binPath, $bytes)
$json = $gltf | ConvertTo-Json -Depth 100
[System.IO.File]::WriteAllText($GltfPath, $json, [System.Text.UTF8Encoding]::new($false))

Write-Host "Baked scale into Nash POSITION accessors: $($positionAccessors.Count)"
Write-Host "Normalized Nash nodes: $($targets.Count)"
Write-Host "Scale baked into vertices: $Scale"
Write-Host "Backup: $backupDir"
