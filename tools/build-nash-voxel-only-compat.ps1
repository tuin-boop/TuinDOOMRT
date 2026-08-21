param(
    [string]$InstallRoot = 'C:\doomrttest600'
)

$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$stage = Join-Path $workspace 'compat\nashgore-voxel-only'
$sprites = Join-Path $stage 'sprites'
$package = Join-Path $workspace 'compat\nashgore_rt_voxel_compat-repaired.pk3'
$installed = Join-Path $InstallRoot 'Game\Mods\nashgore_rt_voxel_compat.pk3'

$unsupportedFrames = @(
    'BDRPA0', 'BDRPB0', 'BDRPC0', 'BDRPD0', 'BDRPE0', 'BDRPF0',
    'BLIMA0', 'BLIMB0', 'BLIMC0', 'BLIMD0', 'BLIME0',
    'NBL1A0', 'NBL1B0', 'NBL1C0', 'NBL1D0', 'NBL1E0', 'NBL1F0',
    'NBL2A0', 'NBL2B0', 'NBL2C0', 'NBL2D0', 'NBL3A0', 'NLIQA0',
    'NSPTA0', 'NSPTB0', 'NSPTC0', 'NSPTD0',
    'NTRLA0', 'NTRLB0', 'NTRLC0', 'NTRLD0'
)

[IO.Directory]::CreateDirectory($sprites) | Out-Null
Add-Type -AssemblyName System.Drawing
$blank = Join-Path $sprites '_blank.png'
$bitmap = [System.Drawing.Bitmap]::new(
    1,
    1,
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
)
try {
    $bitmap.SetPixel(0, 0, [System.Drawing.Color]::Transparent)
    $bitmap.Save($blank, [System.Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $bitmap.Dispose()
}

foreach ($frame in $unsupportedFrames) {
    [IO.File]::Copy($blank, (Join-Path $sprites ($frame + '.png')), $true)
}
[IO.File]::Delete($blank)

if ([IO.File]::Exists($package)) {
    [IO.File]::Delete($package)
}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $package -CompressionLevel Optimal
[IO.File]::Copy($package, $installed, $true)

Write-Output "Suppressed sprite-only Nash frames: $($unsupportedFrames.Count)"
Write-Output "Package: $package"
Write-Output "Installed: $installed"
