param(
    [string]$GameDir = 'C:\doomrttest600\Game'
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$nashPath = Join-Path $GameDir 'Mods\nashgore.pk3'
$compatPath = Join-Path $GameDir 'Mods\nashgore_rt_voxel_compat.pk3'
if (!(Test-Path -LiteralPath $nashPath) -or !(Test-Path -LiteralPath $compatPath)) {
    throw 'The NashGore package or its RT voxel compatibility package is missing.'
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backupDir = Join-Path $GameDir "VoxelConversion\before-nash-blood-sprite-removal-$stamp"
New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
Copy-Item -LiteralPath $nashPath -Destination (Join-Path $backupDir 'nashgore.pk3')

$compat = [IO.Compression.ZipFile]::OpenRead($compatPath)
try {
    $templateEntry = $compat.GetEntry('sprites/BDRPA0.png')
    if ($null -eq $templateEntry) { throw 'Transparent compatibility sprite template is missing.' }
    $memory = [IO.MemoryStream]::new()
    $stream = $templateEntry.Open()
    try { $stream.CopyTo($memory) } finally { $stream.Dispose() }
    $transparentPng = $memory.ToArray()
    $memory.Dispose()
} finally { $compat.Dispose() }

$names = @(
    'BDRPA0','BDRPB0','BDRPC0','BDRPD0','BDRPE0','BDRPF0',
    'BLIMA0','BLIMB0','BLIMC0','BLIMD0','BLIME0',
    'NBL1A0','NBL1B0','NBL1C0','NBL1D0','NBL1E0','NBL1F0',
    'NBL2A0','NBL2B0','NBL2C0','NBL2D0','NBL3A0','NLIQA0',
    'NSPTA0','NSPTB0','NSPTC0','NSPTD0','NTRLA0','NTRLB0','NTRLC0','NTRLD0'
)

$zip = [IO.Compression.ZipFile]::Open($nashPath, [IO.Compression.ZipArchiveMode]::Update)
try {
    $changed = 0
    foreach ($name in $names) {
        $entry = $zip.GetEntry("sprites/blood/$name.png")
        if ($null -eq $entry) { continue }
        $entry.Delete()
        $replacement = $zip.CreateEntry("sprites/blood/$name.png", [IO.Compression.CompressionLevel]::Optimal)
        $out = $replacement.Open()
        try { $out.Write($transparentPng, 0, $transparentPng.Length) } finally { $out.Dispose() }
        $changed++
    }
} finally { $zip.Dispose() }

Write-Host "Replaced $changed flat blood sprite frames with transparent frames."
Write-Host 'Voxel gib sprites and NashGore behavior were left intact.'
Write-Host "Backup: $backupDir"

