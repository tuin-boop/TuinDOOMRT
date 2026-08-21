param(
    [Parameter(Mandatory = $true)]
    [string]$GltfPath,

    [double]$Scale = (1.0 / 32.0)
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $GltfPath -PathType Leaf)) {
    throw "GLTF not found: $GltfPath"
}

$gltf = Get-Content -Raw -LiteralPath $GltfPath | ConvertFrom-Json
$targetPattern = '^(CRS[123]A|GIB[0-9][AB]|SGC[12][AB]|SGW[12][AB]|SQSHA|ICEN[ABCD])$'
$targets = @($gltf.nodes | Where-Object { $_.name -match $targetPattern })

if ($targets.Count -ne 36) {
    throw "Expected 36 Nash voxel nodes, found $($targets.Count)."
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$rtRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $GltfPath))
$backupDir = Join-Path $rtRoot "VoxelConversion\before-nash-scale-$stamp"
New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
Copy-Item -LiteralPath $GltfPath -Destination (Join-Path $backupDir (Split-Path -Leaf $GltfPath))

foreach ($node in $targets) {
    $node.scale = @($Scale, $Scale, $Scale)
}

$json = $gltf | ConvertTo-Json -Depth 100
[System.IO.File]::WriteAllText($GltfPath, $json, [System.Text.UTF8Encoding]::new($false))

Write-Host "Normalized Nash voxel nodes: $($targets.Count)"
Write-Host "Scale: $Scale (1/32 Doom-unit to RT-world conversion)"
Write-Host "Backup: $backupDir"
