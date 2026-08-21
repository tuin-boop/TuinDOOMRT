param(
    [Parameter(Mandatory = $true)]
    [string]$GltfPath
)

$ErrorActionPreference = 'Stop'
$gltfFullPath = [IO.Path]::GetFullPath($GltfPath)
$gltfDirectory = [IO.Path]::GetDirectoryName($gltfFullPath)
$document = Get-Content -LiteralPath $gltfFullPath -Raw | ConvertFrom-Json -AsHashtable

if ($document.extras -and $document.extras.tuindoomFacingCorrection -eq 'baked-z180') {
    throw "The 180-degree facing correction is already baked into $gltfFullPath"
}

$bufferPath = Join-Path $gltfDirectory $document.buffers[0].uri
$bytes = [IO.File]::ReadAllBytes($bufferPath)
$attributeKinds = @{}

Add-Type -TypeDefinition @'
public static class TuinDoomGltfTransform
{
    public static void NegateXY(byte[] bytes, long baseOffset, int stride, int count)
    {
        for (int i = 0; i < count; i++)
        {
            long elementOffset = baseOffset + ((long)i * stride);
            // glTF FLOAT data is little-endian IEEE-754. Flipping the sign bit
            // avoids millions of temporary float/byte-array allocations.
            bytes[checked((int)(elementOffset + 3))] ^= 0x80;
            bytes[checked((int)(elementOffset + 7))] ^= 0x80;
        }
    }
}
'@

foreach ($mesh in $document.meshes) {
    foreach ($primitive in $mesh.primitives) {
        foreach ($semantic in @('POSITION', 'NORMAL', 'TANGENT')) {
            if ($primitive.attributes.ContainsKey($semantic)) {
                $attributeKinds[[int]$primitive.attributes[$semantic]] = $semantic
            }
        }
    }
}

function Get-ComponentCount([string]$type) {
    switch ($type) {
        'VEC3' { return 3 }
        'VEC4' { return 4 }
        default { throw "Unsupported accessor type: $type" }
    }
}

foreach ($entry in $attributeKinds.GetEnumerator()) {
    $accessorIndex = [int]$entry.Key
    $semantic = [string]$entry.Value
    $accessor = $document.accessors[$accessorIndex]
    if ([int]$accessor.componentType -ne 5126) {
        throw "Accessor $accessorIndex ($semantic) is not FLOAT"
    }

    $componentCount = Get-ComponentCount ([string]$accessor.type)
    $view = $document.bufferViews[[int]$accessor.bufferView]
    $viewOffset = if ($view.ContainsKey('byteOffset')) { [int64]$view.byteOffset } else { 0 }
    $accessorOffset = if ($accessor.ContainsKey('byteOffset')) { [int64]$accessor.byteOffset } else { 0 }
    $stride = if ($view.ContainsKey('byteStride')) { [int]$view.byteStride } else { $componentCount * 4 }
    $baseOffset = $viewOffset + $accessorOffset

    [TuinDoomGltfTransform]::NegateXY($bytes, $baseOffset, $stride, [int]$accessor.count)

    if ($semantic -eq 'POSITION' -and $accessor.ContainsKey('min') -and $accessor.ContainsKey('max')) {
        $oldMin = @($accessor.min)
        $oldMax = @($accessor.max)
        $accessor.min = @(-[double]$oldMax[0], -[double]$oldMax[1], [double]$oldMin[2])
        $accessor.max = @(-[double]$oldMin[0], -[double]$oldMin[1], [double]$oldMax[2])
    }
}

if (-not $document.ContainsKey('extras')) { $document.extras = @{} }
$document.extras.tuindoomFacingCorrection = 'baked-z180'

[IO.File]::WriteAllBytes($bufferPath, $bytes)
$json = $document | ConvertTo-Json -Depth 100
[IO.File]::WriteAllText($gltfFullPath, $json, [Text.UTF8Encoding]::new($false))
Write-Host "Baked Z-axis 180-degree correction into $($attributeKinds.Count) accessors."
