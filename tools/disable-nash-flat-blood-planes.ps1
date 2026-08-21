param(
    [string]$GameDir = 'C:\doomrttest600\Game'
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$nashPath = Join-Path $GameDir 'Mods\nashgore.pk3'
if (!(Test-Path -LiteralPath $nashPath)) {
    throw "NashGore package not found: $nashPath"
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backupDir = Join-Path $GameDir "VoxelConversion\before-nash-flat-blood-removal-$stamp"
New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
Copy-Item -LiteralPath $nashPath -Destination (Join-Path $backupDir 'nashgore.pk3')

$entryName = 'zscript/NashGoreBloodPlane.zc'
$zip = [IO.Compression.ZipFile]::Open($nashPath, [IO.Compression.ZipArchiveMode]::Update)
try {
    $entry = $zip.GetEntry($entryName)
    if ($null -eq $entry) {
        $entry = $zip.Entries | Where-Object { $_.FullName -like '*NashGoreBloodPlane.zc' } | Select-Object -First 1
    }
    if ($null -eq $entry) { throw 'NashGoreBloodPlane.zc was not found in the package.' }

    $reader = [IO.StreamReader]::new($entry.Open())
    try { $source = $reader.ReadToEnd() } finally { $reader.Dispose() }

    $original = @'
	override void BeginPlay(void)
	{
		ChangeStatNum(STAT_NashGore_Gore);
		NashGoreGameplayStatics.QueueGore();
		rndzoffset = frandom[rnd_SpawnBloodPlane](0.0001, 0.0100);
		A_SetScale(frandom[rnd_SpawnBloodPlane](0.9, 1.0));
		Super.BeginPlay();

		if (!Level.IsPointInLevel(Pos))
		{
			Destroy();
			return;
		}
	}
'@

    $replacement = @'
	override void BeginPlay(void)
	{
		// TuinDoom RT: keep NashGore's voxel chunks, but suppress its flat
		// floor pools, trails and footprints. These are model-backed blood
		// planes (NGMV), not the ordinary PNG particle sprites.
		Destroy();
		return;
	}
'@

    if ($source.Contains('TuinDoom RT: keep NashGore')) {
        Write-Host 'Flat NashGore blood planes are already disabled.'
        Write-Host "Backup: $backupDir"
        return
    }

    $normalized = $source -replace "`r`n", "`n"
    $originalNormalized = $original -replace "`r`n", "`n"
    $replacementNormalized = $replacement -replace "`r`n", "`n"
    if (!$normalized.Contains($originalNormalized)) {
        throw 'The expected NashGoreBloodPlane BeginPlay block was not found; no changes were made.'
    }
    $updated = $normalized.Replace($originalNormalized, $replacementNormalized)

    $storedName = $entry.FullName
    $entry.Delete()
    $newEntry = $zip.CreateEntry($storedName, [IO.Compression.CompressionLevel]::Optimal)
    $writer = [IO.StreamWriter]::new($newEntry.Open(), [Text.UTF8Encoding]::new($false))
    try { $writer.Write($updated) } finally { $writer.Dispose() }
} finally {
    $zip.Dispose()
}

# Read the package back to catch a damaged ZIP or failed replacement immediately.
$verify = [IO.Compression.ZipFile]::OpenRead($nashPath)
try {
    $entry = $verify.GetEntry($entryName)
    if ($null -eq $entry) {
        $entry = $verify.Entries | Where-Object { $_.FullName -like '*NashGoreBloodPlane.zc' } | Select-Object -First 1
    }
    $reader = [IO.StreamReader]::new($entry.Open())
    try { $verifiedSource = $reader.ReadToEnd() } finally { $reader.Dispose() }
    if (!$verifiedSource.Contains('TuinDoom RT: keep NashGore')) {
        throw 'Verification failed: the blood-plane patch is absent.'
    }
} finally {
    $verify.Dispose()
}

Write-Host 'Disabled NashGore flat blood pools, trails and footprints.'
Write-Host 'Voxel chunks and other NashGore effects were left intact.'
Write-Host "Backup: $backupDir"
