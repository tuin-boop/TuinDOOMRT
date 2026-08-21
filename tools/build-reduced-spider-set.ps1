param(
    [Parameter(Mandatory = $true)]
    [string]$SourceGltf,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [string]$OutputStem = "set_2_spid"
)

$ErrorActionPreference = "Stop"

$sourceGltfPath = (Resolve-Path -LiteralPath $SourceGltf).Path
$sourceDirectory = Split-Path -Parent $sourceGltfPath
$document = Get-Content -LiteralPath $sourceGltfPath -Raw | ConvertFrom-Json -AsHashtable

if ($document.meshes.Count -lt 19 -or $document.nodes.Count -lt 20) {
    throw "The source is not the expected 19-frame Spider Mastermind glTF."
}

$sourceBinPath = Join-Path $sourceDirectory $document.buffers[0].uri
if (-not (Test-Path -LiteralPath $sourceBinPath -PathType Leaf)) {
    throw "Source binary buffer was not found: $sourceBinPath"
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$outputGltfPath = Join-Path $OutputDirectory ($OutputStem + ".gltf")
$outputBinPath = Join-Path $OutputDirectory ($OutputStem + ".bin")

# Live/combat frames A-I are the comparatively small, complete part of the model.
# Death frames J-S account for most of the unfinished model's ~506 MB payload.
$keptMeshes = @($document.meshes[0..8])

$usedAccessorIndices = [System.Collections.Generic.SortedSet[int]]::new()
foreach ($mesh in $keptMeshes) {
    foreach ($primitive in $mesh.primitives) {
        if ($primitive.ContainsKey("indices")) {
            [void]$usedAccessorIndices.Add([int]$primitive.indices)
        }
        foreach ($attributeIndex in $primitive.attributes.Values) {
            [void]$usedAccessorIndices.Add([int]$attributeIndex)
        }
    }
}

$accessorMap = @{}
$newAccessors = [System.Collections.ArrayList]::new()
foreach ($oldAccessorIndex in $usedAccessorIndices) {
    $accessorMap[$oldAccessorIndex] = $newAccessors.Count
    [void]$newAccessors.Add($document.accessors[$oldAccessorIndex])
}

$usedViewIndices = [System.Collections.Generic.SortedSet[int]]::new()
foreach ($accessor in $newAccessors) {
    if ($accessor.ContainsKey("bufferView")) {
        [void]$usedViewIndices.Add([int]$accessor.bufferView)
    }
}

$viewMap = @{}
$newViews = [System.Collections.ArrayList]::new()
$sourceStream = [System.IO.File]::OpenRead($sourceBinPath)
$outputStream = [System.IO.File]::Create($outputBinPath)
try {
    foreach ($oldViewIndex in $usedViewIndices) {
        while (($outputStream.Position % 4) -ne 0) {
            $outputStream.WriteByte(0)
        }

        $oldView = $document.bufferViews[$oldViewIndex]
        $newView = @{}
        foreach ($entry in $oldView.GetEnumerator()) {
            $newView[$entry.Key] = $entry.Value
        }
        $newView.buffer = 0
        $newView.byteOffset = $outputStream.Position

        $viewMap[$oldViewIndex] = $newViews.Count
        [void]$newViews.Add($newView)

        $sourceOffset = if ($oldView.ContainsKey("byteOffset")) { [int64]$oldView.byteOffset } else { 0 }
        $remaining = [int64]$oldView.byteLength
        $sourceStream.Position = $sourceOffset
        $copyBuffer = New-Object byte[] 1048576
        while ($remaining -gt 0) {
            $request = [int][Math]::Min($copyBuffer.Length, $remaining)
            $read = $sourceStream.Read($copyBuffer, 0, $request)
            if ($read -le 0) {
                throw "Unexpected end of source buffer while copying bufferView $oldViewIndex."
            }
            $outputStream.Write($copyBuffer, 0, $read)
            $remaining -= $read
        }
    }
}
finally {
    $sourceStream.Dispose()
    $outputStream.Dispose()
}

foreach ($accessor in $newAccessors) {
    if ($accessor.ContainsKey("bufferView")) {
        $accessor.bufferView = $viewMap[[int]$accessor.bufferView]
    }
}

foreach ($mesh in $keptMeshes) {
    foreach ($primitive in $mesh.primitives) {
        if ($primitive.ContainsKey("indices")) {
            $primitive.indices = $accessorMap[[int]$primitive.indices]
        }
        foreach ($attributeName in @($primitive.attributes.Keys)) {
            $primitive.attributes[$attributeName] = $accessorMap[[int]$primitive.attributes[$attributeName]]
        }
    }
}

# Preserve all 19 sprite-frame node names. J-S intentionally reuse pain frame I,
# so the stock death state machine, death sound, timing, and boss trigger still run.
$newNodes = [System.Collections.ArrayList]::new()
$painNode = $document.nodes[8]
for ($frameIndex = 0; $frameIndex -lt 19; $frameIndex++) {
    $sourceNode = $document.nodes[$frameIndex]
    $node = @{}
    foreach ($entry in $sourceNode.GetEnumerator()) {
        $node[$entry.Key] = $entry.Value
    }
    if ($frameIndex -ge 9) {
        $node.mesh = 8
        if ($painNode.ContainsKey("translation")) {
            $node.translation = @($painNode.translation)
        }
        if ($painNode.ContainsKey("rotation")) {
            $node.rotation = @($painNode.rotation)
        }
        if ($painNode.ContainsKey("scale")) {
            $node.scale = @($painNode.scale)
        }
    }

    # The shared set root already supplies the RT coordinate conversion. A
    # rotation on each frame applies that conversion twice and makes the actor
    # face backward in game. Keep frame nodes unrotated, like the working
    # Cyberdemon set and the converter's untouched Spider set.
    [void]$node.Remove("rotation")
    [void]$newNodes.Add($node)
}

$rootNode = @{}
foreach ($entry in $document.nodes[19].GetEnumerator()) {
    $rootNode[$entry.Key] = $entry.Value
}
$rootNode.children = @(0..18)
[void]$newNodes.Add($rootNode)

$document.meshes = $keptMeshes
$document.accessors = @($newAccessors)
$document.bufferViews = @($newViews)
$document.nodes = @($newNodes)
$document.scenes[0].nodes = @(19)
$document.buffers = @(@{
    byteLength = (Get-Item -LiteralPath $outputBinPath).Length
    uri = ($OutputStem + ".bin")
})

$document.asset.generator = "TuinDOOM RT reduced Spider Mastermind builder"
$document | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $outputGltfPath -Encoding utf8

[pscustomobject]@{
    Gltf = $outputGltfPath
    Bin = $outputBinPath
    Frames = 19
    UniqueMeshes = $keptMeshes.Count
    Bytes = (Get-Item -LiteralPath $outputBinPath).Length
}
