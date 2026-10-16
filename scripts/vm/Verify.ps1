# Built-in PowerShell/.NET only. Creates and destroys only IDs returned by this run.
$ErrorActionPreference = 'Stop'
$cli = Join-Path $PSScriptRoot 'memdisk.exe'
function Invoke-MemDisk([string[]]$Arguments) {
    $text = & $cli @Arguments --json
    if ($LASTEXITCODE) { throw ($text -join "`n") }
    return (($text -join "`n") | ConvertFrom-Json)
}
& (Join-Path $PSScriptRoot 'Check-Package.ps1')
$initial = Invoke-MemDisk -Arguments @('list')
foreach ($fs in @('ntfs','exfat')) {
    $id = $null
    try {
        $reply = Invoke-MemDisk -Arguments @('create','--mib','64','--fs',$fs,'--label','Verify')
        $before = @($initial.disks | ForEach-Object { $_.id })
        $created = @($reply.disks | Where-Object { $_.id -notin $before })
        if ($created.Count -ne 1) { throw 'Expected exactly one new instance.' }
        $disk = $created[0]; $id = $disk.id
        & (Join-Path $PSScriptRoot 'memdisk-access-test.exe') --restricted $disk.letter
        if ($LASTEXITCODE) { throw 'Ordinary-token access/security regression failed.' }
        $path = "$($disk.letter):\memdisk-verification.bin"
        $data = New-Object byte[] (4 * 1024 * 1024)
        ([Random]::new(20261016)).NextBytes($data)
        [IO.File]::WriteAllBytes($path, $data)
        $expected = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        $actual = [IO.File]::ReadAllBytes($path)
        $sha = [Security.Cryptography.SHA256]::Create()
        $sourceHash = [BitConverter]::ToString($sha.ComputeHash($data))
        $readHash = [BitConverter]::ToString($sha.ComputeHash($actual))
        $sha.Dispose()
        if ($sourceHash -ne $readHash) { throw 'Read/write data mismatch.' }
        & (Join-Path $PSScriptRoot 'Test-BusyVolume.ps1') -Letter $disk.letter -Id "$id"
        $null = Invoke-MemDisk -Arguments @('unmount',"$id")
        $null = Invoke-MemDisk -Arguments @('mount',"$id")
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $expected) { throw 'Remount data mismatch.' }
        Remove-Item -LiteralPath $path
        $null = Invoke-MemDisk -Arguments @('destroy',"$id",'--yes'); $id = $null
        Write-Host "PASS: $fs create, write, read, hash, unmount/remount, destroy."
    } finally {
        if ($null -ne $id) { Write-Warning "Instance $id retained for diagnosis. Destroy explicitly after closing handles." }
    }
}
$final = Invoke-MemDisk -Arguments @('list')
if (@($final.disks).Count -ne @($initial.disks).Count) { throw 'Instance count did not return to baseline.' }
Write-Host 'PASS: verification completed. This is a smoke test, not a product stability certification.'
