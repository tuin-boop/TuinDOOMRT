param(
    [string]$InstallRoot = 'C:\doomrttest600',
    [string]$SetName = 'set_0_misc'
)

$ErrorActionPreference = 'Stop'
$replace = Join-Path $InstallRoot 'Game\rt\replace'
$gltfPath = Join-Path $replace ($SetName + '.gltf')
$json = Get-Content -LiteralPath $gltfPath -Raw | ConvertFrom-Json

if ($json.buffers.Count -le 1) {
    Write-Output "$SetName already uses a single buffer."
    exit 0
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backup = Join-Path $InstallRoot ('VoxelConversion\before-buffer-flatten-' + $stamp)
[IO.Directory]::CreateDirectory($backup) | Out-Null
[IO.File]::Copy($gltfPath, (Join-Path $backup ($SetName + '.gltf')), $true)
$primaryPath = Join-Path $replace $json.buffers[0].uri
[IO.File]::Copy($primaryPath, (Join-Path $backup $json.buffers[0].uri), $true)

$tempPath = $primaryPath + '.flattening'
$output = [IO.File]::Open($tempPath, [IO.FileMode]::Create, [IO.FileAccess]::Write)
$offsets = [long[]]::new($json.buffers.Count)
try {
    for ($bufferIndex = 0; $bufferIndex -lt $json.buffers.Count; $bufferIndex++) {
        while (($output.Position % 4) -ne 0) {
            $output.WriteByte(0)
        }
        $offsets[$bufferIndex] = $output.Position
        $sourcePath = Join-Path $replace $json.buffers[$bufferIndex].uri
        $input = [IO.File]::OpenRead($sourcePath)
        try {
            $input.CopyTo($output)
        }
        finally {
            $input.Dispose()
        }
    }
}
finally {
    $output.Dispose()
}

foreach ($view in $json.bufferViews) {
    $oldBuffer = [int]$view.buffer
    $oldOffset = if ($null -eq $view.byteOffset) { 0L } else { [long]$view.byteOffset }
    $view.buffer = 0
    $newOffset = $offsets[$oldBuffer] + $oldOffset
    if ($null -eq $view.PSObject.Properties['byteOffset']) {
        $view | Add-Member -NotePropertyName byteOffset -NotePropertyValue $newOffset
    }
    else {
        $view.byteOffset = $newOffset
    }
}

$totalLength = (Get-Item -LiteralPath $tempPath).Length
$json.buffers = @([pscustomobject]@{
    byteLength = $totalLength
    uri = ($SetName + '.bin')
})

$json | ConvertTo-Json -Depth 100 -Compress | Set-Content -LiteralPath $gltfPath -Encoding utf8NoBOM
[IO.File]::Copy($tempPath, $primaryPath, $true)
[IO.File]::Delete($tempPath)

Write-Output "Flattened $($offsets.Count) buffers into $($json.buffers[0].uri)."
Write-Output "Final byte length: $totalLength"
Write-Output "Backup: $backup"
