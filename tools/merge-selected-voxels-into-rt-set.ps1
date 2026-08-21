param(
    [string]$InstallRoot = "C:\doomrttest600",
    [string]$TargetSet = "set_0_misc.gltf",
    [string[]]$Models = @(
        "BAR1A", "BAR1B",
        "CRS1A", "CRS2A", "CRS3A",
        "GIB0A", "GIB0B", "GIB1A", "GIB1B", "GIB2A", "GIB2B",
        "GIB3A", "GIB3B", "GIB4A", "GIB4B", "GIB5A", "GIB5B",
        "GIB6A", "GIB6B", "GIB7A", "GIB7B", "GIB8A", "GIB8B",
        "GIB9A", "GIB9B",
        "SGC1A", "SGC1B", "SGC2A", "SGC2B",
        "SGW1A", "SGW1B", "SGW2A", "SGW2B",
        "SQSHA", "ICENA", "ICENB", "ICENC", "ICEND"
    )
)

$ErrorActionPreference = "Stop"

$install = [System.IO.Path]::GetFullPath($InstallRoot)
$game = Join-Path $install "Game"
$source = Join-Path $game "vox2gltf"
$replace = Join-Path $game "rt\replace"
$targetPath = Join-Path $replace $TargetSet
$backupRoot = Join-Path $install "VoxelConversion"

if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "Converted voxel directory not found: $source"
}
if (-not (Test-Path -LiteralPath $targetPath -PathType Leaf)) {
    throw "Target RT model set not found: $targetPath"
}
if (-not $targetPath.StartsWith($game, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to modify a model set outside the selected installation."
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$backup = Join-Path $backupRoot "before-nash-voxel-repair-$stamp"
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath $replace -Destination (Join-Path $backup "replace") -Recurse -Force
$compat = Join-Path $game "Mods\nashgore_rt_voxel_compat.pk3"
if (Test-Path -LiteralPath $compat -PathType Leaf) {
    Copy-Item -LiteralPath $compat -Destination $backup -Force
}

$target = Get-Content -LiteralPath $targetPath -Raw | ConvertFrom-Json
$mainRootIndex = -1
for ($i = 0; $i -lt $target.nodes.Count; $i++) {
    if ($target.nodes[$i].name -eq "rtgl1_main_root") {
        $mainRootIndex = $i
        break
    }
}
if ($mainRootIndex -lt 0) { throw "The target set has no rtgl1_main_root node." }

$existingNames = @{}
foreach ($node in $target.nodes) {
    if ($null -ne $node.name) { $existingNames[$node.name.ToUpperInvariant()] = $true }
}
foreach ($name in $Models) {
    if ($existingNames.ContainsKey($name.ToUpperInvariant())) {
        throw "The target already contains a node named $name; restore the backup before retrying."
    }
}

$accessors = [System.Collections.ArrayList]@($target.accessors)
$bufferViews = [System.Collections.ArrayList]@($target.bufferViews)
$buffers = [System.Collections.ArrayList]@($target.buffers)
$images = [System.Collections.ArrayList]@($target.images)
$materials = [System.Collections.ArrayList]@($target.materials)
$meshes = [System.Collections.ArrayList]@($target.meshes)
$nodes = [System.Collections.ArrayList]@($target.nodes)
$samplers = [System.Collections.ArrayList]@($target.samplers)
$textures = [System.Collections.ArrayList]@($target.textures)
$newNodeIndexes = [System.Collections.ArrayList]::new()

foreach ($modelName in $Models) {
    $gltfPath = Join-Path $source "$modelName.gltf"
    $binPath = Join-Path $source "$modelName.bin"
    $texturePath = Join-Path $source "vx_$modelName.tga"
    foreach ($required in @($gltfPath, $binPath, $texturePath)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
            throw "Missing converted voxel resource: $required"
        }
    }

    $model = Get-Content -LiteralPath $gltfPath -Raw | ConvertFrom-Json
    if ($model.nodes.Count -ne 1 -or $null -eq $model.nodes[0].mesh) {
        throw "Unexpected converted-model layout: $gltfPath"
    }

    $accessorOffset = $accessors.Count
    $bufferViewOffset = $bufferViews.Count
    $bufferOffset = $buffers.Count
    $imageOffset = $images.Count
    $materialOffset = $materials.Count
    $meshOffset = $meshes.Count
    $samplerOffset = $samplers.Count
    $textureOffset = $textures.Count

    foreach ($item in $model.accessors) {
        if ($null -ne $item.bufferView) { $item.bufferView = [int]$item.bufferView + $bufferViewOffset }
        [void]$accessors.Add($item)
    }
    foreach ($item in $model.bufferViews) {
        $item.buffer = [int]$item.buffer + $bufferOffset
        [void]$bufferViews.Add($item)
    }
    foreach ($item in $model.buffers) { [void]$buffers.Add($item) }
    foreach ($item in $model.images) { [void]$images.Add($item) }
    foreach ($item in $model.samplers) { [void]$samplers.Add($item) }
    foreach ($item in $model.textures) {
        if ($null -ne $item.source) { $item.source = [int]$item.source + $imageOffset }
        if ($null -ne $item.sampler) { $item.sampler = [int]$item.sampler + $samplerOffset }
        [void]$textures.Add($item)
    }
    foreach ($item in $model.materials) {
        if ($null -ne $item.pbrMetallicRoughness.baseColorTexture.index) {
            $item.pbrMetallicRoughness.baseColorTexture.index = [int]$item.pbrMetallicRoughness.baseColorTexture.index + $textureOffset
        }
        [void]$materials.Add($item)
    }
    foreach ($item in $model.meshes) {
        foreach ($primitive in $item.primitives) {
            if ($null -ne $primitive.indices) { $primitive.indices = [int]$primitive.indices + $accessorOffset }
            if ($null -ne $primitive.material) { $primitive.material = [int]$primitive.material + $materialOffset }
            foreach ($attribute in $primitive.attributes.PSObject.Properties) {
                $attribute.Value = [int]$attribute.Value + $accessorOffset
            }
        }
        [void]$meshes.Add($item)
    }

    $node = $model.nodes[0]
    $node.name = $modelName
    $node.mesh = [int]$node.mesh + $meshOffset
    $nodeIndex = $nodes.Count
    [void]$nodes.Add($node)
    [void]$newNodeIndexes.Add($nodeIndex)

    Copy-Item -LiteralPath $binPath -Destination (Join-Path $replace "$modelName.bin") -Force
    Copy-Item -LiteralPath $texturePath -Destination (Join-Path $replace "vx_$modelName.tga") -Force
}

$target.accessors = @($accessors)
$target.bufferViews = @($bufferViews)
$target.buffers = @($buffers)
$target.images = @($images)
$target.materials = @($materials)
$target.meshes = @($meshes)
$target.nodes = @($nodes)
$target.samplers = @($samplers)
$target.textures = @($textures)
$target.nodes[$mainRootIndex].children = @($target.nodes[$mainRootIndex].children) + @($newNodeIndexes)

$target | ConvertTo-Json -Depth 100 -Compress | Set-Content -LiteralPath $targetPath -Encoding UTF8

$standalone = Join-Path $replace "set_0_000_voxels.gltf"
if (Test-Path -LiteralPath $standalone -PathType Leaf) {
    Remove-Item -LiteralPath $standalone -Force
}

$check = Get-Content -LiteralPath $targetPath -Raw | ConvertFrom-Json
$names = @($check.nodes | ForEach-Object { $_.name })
foreach ($name in $Models) {
    if ($names -notcontains $name) { throw "Validation failed: node $name is missing." }
}
foreach ($buffer in $check.buffers) {
    if (-not (Test-Path -LiteralPath (Join-Path $replace $buffer.uri) -PathType Leaf)) {
        throw "Validation failed: missing buffer $($buffer.uri)."
    }
}
foreach ($image in $check.images) {
    if (-not (Test-Path -LiteralPath (Join-Path $replace $image.uri) -PathType Leaf)) {
        throw "Validation failed: missing texture $($image.uri)."
    }
}

Write-Host "Merged $($Models.Count) voxel frames into $TargetSet."
Write-Host "Backup: $backup"
