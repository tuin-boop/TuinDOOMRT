param(
    [string]$InstallRoot = "C:\doomrttest600",
    [string]$SourceDir = "",
    [string]$ReplaceDir = "",
    [string]$TargetSet = "set_2_spid.gltf",
    [string[]]$Models = @(
        "SPIDA", "SPIDB", "SPIDC", "SPIDD", "SPIDE", "SPIDF", "SPIDG",
        "SPIDH", "SPIDI", "SPIDJ", "SPIDK", "SPIDL", "SPIDM", "SPIDN",
        "SPIDO", "SPIDP", "SPIDQ", "SPIDR", "SPIDS"
    )
)

$ErrorActionPreference = "Stop"

$install = [System.IO.Path]::GetFullPath($InstallRoot)
if ([string]::IsNullOrWhiteSpace($SourceDir)) {
    $SourceDir = Join-Path $install "Game\vox2gltf"
}
if ([string]::IsNullOrWhiteSpace($ReplaceDir)) {
    $ReplaceDir = Join-Path $install "Game\rt\replace"
}
$source = [System.IO.Path]::GetFullPath($SourceDir)
$replace = [System.IO.Path]::GetFullPath($ReplaceDir)

if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "Converted voxel directory not found: $source"
}
if (-not (Test-Path -LiteralPath $replace -PathType Container)) {
    throw "RT replacement directory not found: $replace"
}
if ([System.IO.Path]::GetFileName($TargetSet) -ne $TargetSet -or -not $TargetSet.EndsWith(".gltf")) {
    throw "TargetSet must be a simple .gltf filename."
}

$converted = foreach ($model in $Models) {
    $path = Join-Path $source "$model.gltf"
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing converted model: $path"
    }
    Get-Item -LiteralPath $path
}

$merged = [ordered]@{
    accessors   = [System.Collections.ArrayList]::new()
    asset       = [ordered]@{ generator = "TuinDOOM RT dedicated voxel-set merger"; version = "2.0" }
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
    $modelName = $gltfFile.BaseName
    $binSource = Join-Path $source "$modelName.bin"
    $textureSource = Join-Path $source "vx_$modelName.tga"
    if (-not (Test-Path -LiteralPath $binSource -PathType Leaf)) { throw "Missing $binSource" }
    if (-not (Test-Path -LiteralPath $textureSource -PathType Leaf)) { throw "Missing $textureSource" }

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

    Copy-Item -LiteralPath $binSource -Destination (Join-Path $replace "$modelName.bin") -Force
    Copy-Item -LiteralPath $textureSource -Destination (Join-Path $replace "vx_$modelName.tga") -Force
}

$children = @(0..($converted.Count - 1))
[void]$merged.nodes.Add([ordered]@{ name = "rtgl1_main_root"; children = $children })

$outputPath = Join-Path $replace $TargetSet
$merged | ConvertTo-Json -Depth 100 -Compress | Set-Content -LiteralPath $outputPath -Encoding UTF8

$check = Get-Content -LiteralPath $outputPath -Raw | ConvertFrom-Json
$rootIndex = [int]$check.scenes[0].nodes[0]
if ($check.nodes.Count -ne ($converted.Count + 1) -or $check.meshes.Count -ne $converted.Count) {
    throw "Dedicated set validation failed: $($check.nodes.Count) nodes, $($check.meshes.Count) meshes."
}
if ($check.nodes[$rootIndex].name -ne "rtgl1_main_root" -or $check.nodes[$rootIndex].children.Count -ne $converted.Count) {
    throw "Invalid dedicated RT replacement root."
}
foreach ($resource in @($check.buffers.uri) + @($check.images.uri)) {
    if (-not (Test-Path -LiteralPath (Join-Path $replace $resource) -PathType Leaf)) {
        throw "Missing resource referenced by dedicated set: $resource"
    }
}

Write-Host "Installed $($converted.Count) model(s) into dedicated RT set: $outputPath"
