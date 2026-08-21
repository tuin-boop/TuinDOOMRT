param(
    [string]$InstallRoot = "C:\doomrttest600",
    [string[]]$Models = @()
)

$ErrorActionPreference = "Stop"

$install = [System.IO.Path]::GetFullPath($InstallRoot)
$source = Join-Path $install "Game\vox2gltf"
$replace = Join-Path $install "Game\rt\replace"
$backupRoot = Join-Path $install "VoxelConversion"

if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "Converted voxel directory not found: $source"
}
if (-not (Test-Path -LiteralPath $replace -PathType Container)) {
    throw "RT replacement directory not found: $replace"
}
if (-not $replace.StartsWith((Join-Path $install "Game"), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to modify a replacement directory outside the selected install."
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$backup = Join-Path $backupRoot "replace-before-voxel-priority-$stamp"
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath $replace -Destination $backup -Recurse -Force

Get-ChildItem -LiteralPath $replace -Filter "set_7_voxel_*.gltf" -File | Remove-Item -Force
$oldMerged = Join-Path $replace "set_0_000_voxels.gltf"
if (Test-Path -LiteralPath $oldMerged -PathType Leaf) {
    Remove-Item -LiteralPath $oldMerged -Force
}
$oldProbe = Join-Path $replace "zz_voxel_gib0a_test.gltf"
if (Test-Path -LiteralPath $oldProbe -PathType Leaf) {
    Remove-Item -LiteralPath $oldProbe -Force
}

$converted = @(Get-ChildItem -LiteralPath $source -Filter "*.gltf" -File | Sort-Object Name)
if ($converted.Count -ne 349) {
    throw "Expected 349 converted voxel models but found $($converted.Count)."
}
if ($Models.Count -gt 0) {
    $wanted = @{}
    foreach ($name in $Models) { $wanted[$name.ToUpperInvariant()] = $true }
    $converted = @($converted | Where-Object { $wanted[$_.BaseName.ToUpperInvariant()] })
    if ($converted.Count -ne $wanted.Count) {
        throw "Could not find every requested probe model. Requested $($wanted.Count), found $($converted.Count)."
    }
}

$merged = [ordered]@{
    accessors   = [System.Collections.ArrayList]::new()
    asset       = [ordered]@{ generator = "TuinDOOM RT voxel merger"; version = "2.0" }
    bufferViews = [System.Collections.ArrayList]::new()
    buffers     = [System.Collections.ArrayList]::new()
    images      = [System.Collections.ArrayList]::new()
    materials   = [System.Collections.ArrayList]::new()
    meshes      = [System.Collections.ArrayList]::new()
    nodes       = [System.Collections.ArrayList]::new()
    samplers    = [System.Collections.ArrayList]::new()
    scene       = 0
    scenes      = @([ordered]@{ nodes = @($converted.Count) })
    textures    = [System.Collections.ArrayList]::new()
}

foreach ($gltfFile in $converted) {
    $modelName = [System.IO.Path]::GetFileNameWithoutExtension($gltfFile.Name)
    $binName = "$modelName.bin"
    $textureName = "vx_$modelName.tga"
    $binSource = Join-Path $source $binName
    $textureSource = Join-Path $source $textureName
    if (-not (Test-Path -LiteralPath $binSource -PathType Leaf)) { throw "Missing $binName" }
    if (-not (Test-Path -LiteralPath $textureSource -PathType Leaf)) { throw "Missing $textureName" }

    $json = Get-Content -LiteralPath $gltfFile.FullName -Raw | ConvertFrom-Json
    if ($json.nodes.Count -ne 1 -or $null -eq $json.nodes[0].mesh) {
        throw "Unexpected node layout in $($gltfFile.Name)"
    }

    $accessorOffset = $merged.accessors.Count
    $bufferViewOffset = $merged.bufferViews.Count
    $bufferOffset = $merged.buffers.Count
    $imageOffset = $merged.images.Count
    $materialOffset = $merged.materials.Count
    $meshOffset = $merged.meshes.Count
    $samplerOffset = $merged.samplers.Count
    $textureOffset = $merged.textures.Count

    foreach ($accessor in $json.accessors) {
        if ($null -ne $accessor.bufferView) { $accessor.bufferView = [int]$accessor.bufferView + $bufferViewOffset }
        [void]$merged.accessors.Add($accessor)
    }
    foreach ($bufferView in $json.bufferViews) {
        $bufferView.buffer = [int]$bufferView.buffer + $bufferOffset
        [void]$merged.bufferViews.Add($bufferView)
    }
    foreach ($buffer in $json.buffers) { [void]$merged.buffers.Add($buffer) }
    foreach ($image in $json.images) { [void]$merged.images.Add($image) }
    foreach ($sampler in $json.samplers) { [void]$merged.samplers.Add($sampler) }
    foreach ($texture in $json.textures) {
        if ($null -ne $texture.source) { $texture.source = [int]$texture.source + $imageOffset }
        if ($null -ne $texture.sampler) { $texture.sampler = [int]$texture.sampler + $samplerOffset }
        [void]$merged.textures.Add($texture)
    }
    foreach ($material in $json.materials) {
        if ($null -ne $material.pbrMetallicRoughness.baseColorTexture.index) {
            $material.pbrMetallicRoughness.baseColorTexture.index = [int]$material.pbrMetallicRoughness.baseColorTexture.index + $textureOffset
        }
        [void]$merged.materials.Add($material)
    }
    foreach ($mesh in $json.meshes) {
        foreach ($primitive in $mesh.primitives) {
            if ($null -ne $primitive.indices) { $primitive.indices = [int]$primitive.indices + $accessorOffset }
            if ($null -ne $primitive.material) { $primitive.material = [int]$primitive.material + $materialOffset }
            foreach ($attribute in $primitive.attributes.PSObject.Properties) {
                $attribute.Value = [int]$attribute.Value + $accessorOffset
            }
        }
        [void]$merged.meshes.Add($mesh)
    }

    $node = $json.nodes[0]
    $node.name = $modelName
    $node.mesh = [int]$node.mesh + $meshOffset
    [void]$merged.nodes.Add($node)

    Copy-Item -LiteralPath $binSource -Destination (Join-Path $replace $binName) -Force
    Copy-Item -LiteralPath $textureSource -Destination (Join-Path $replace $textureName) -Force
}

$children = @(0..($converted.Count - 1))
[void]$merged.nodes.Add([ordered]@{ name = "rtgl1_main_root"; children = $children })

$outputPath = Join-Path $replace "set_0_000_voxels.gltf"
$merged | ConvertTo-Json -Depth 100 -Compress | Set-Content -LiteralPath $outputPath -Encoding UTF8

$check = Get-Content -LiteralPath $outputPath -Raw | ConvertFrom-Json
$rootIndex = [int]$check.scenes[0].nodes[0]
$expectedNodes = $converted.Count + 1
if ($check.nodes.Count -ne $expectedNodes -or $check.meshes.Count -ne $converted.Count) {
    throw "Merged deployment validation failed: $($check.nodes.Count) nodes, $($check.meshes.Count) meshes."
}
if ($check.nodes[$rootIndex].name -ne "rtgl1_main_root" -or $check.nodes[$rootIndex].children.Count -ne $converted.Count) {
    throw "Invalid merged RT replacement root."
}
foreach ($buffer in $check.buffers) {
    if (-not (Test-Path -LiteralPath (Join-Path $replace $buffer.uri) -PathType Leaf)) {
        throw "Missing buffer $($buffer.uri) referenced by the merged replacement."
    }
}
foreach ($image in $check.images) {
    if (-not (Test-Path -LiteralPath (Join-Path $replace $image.uri) -PathType Leaf)) {
        throw "Missing texture $($image.uri) referenced by the merged replacement."
    }
}

Write-Host "Installed one early-priority RT replacement containing $($converted.Count) voxel model(s)."
Write-Host "Output: $outputPath"
Write-Host "Backup: $backup"
