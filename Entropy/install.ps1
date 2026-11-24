$ErrorActionPreference = 'Stop'
$resultPath = Join-Path $PSScriptRoot 'build\install-result.txt'
try {
    $source = Join-Path $PSScriptRoot 'build\Entropy.dll'
    $destination = 'C:\Program Files\NTCore\Explorer Suite\Extensions\CFF Explorer\Entropy'
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    $target = Join-Path $destination 'Entropy.dll'
    # A loaded DLL can usually be renamed even while the host keeps it mapped.
    # Keep the old version as a non-DLL backup; the running host is left untouched.
    $backup = $null
    if (Test-Path -LiteralPath $target) {
        $backup = Join-Path $destination ('Entropy.' + [Guid]::NewGuid().ToString('N') + '.previous')
        Move-Item -LiteralPath $target -Destination $backup
    }
    try {
        Copy-Item -LiteralPath $source -Destination $target
    } catch {
        if ($backup -and !(Test-Path -LiteralPath $target)) {
            Move-Item -LiteralPath $backup -Destination $target
        }
        throw
    }
    if ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $target).Hash) {
        throw 'Installed DLL checksum mismatch.'
    }
    [IO.File]::WriteAllText($resultPath, 'OK: ' + $target)
} catch {
    [IO.File]::WriteAllText($resultPath, 'ERROR: ' + $_.Exception.Message)
    exit 1
}
