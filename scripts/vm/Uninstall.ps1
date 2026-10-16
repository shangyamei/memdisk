$ErrorActionPreference = 'Stop'
$cli = Join-Path $PSScriptRoot 'memdisk.exe'
$raw = & $cli driver-list --json
if ($LASTEXITCODE) { throw 'Cannot establish driver state; do not unload an unknown active driver.' }
$state = $raw | ConvertFrom-Json
if (@($state.disks).Count) { throw 'Active RAM disks exist. Destroy them explicitly before uninstalling.' }
& sc.exe stop MemDiskService
if ($LASTEXITCODE -notin @(0,1062)) { throw 'Cannot stop management service.' }
(Get-Service MemDiskService).WaitForStatus('Stopped',[TimeSpan]::FromSeconds(20))
& sc.exe delete MemDiskService
if ($LASTEXITCODE) { throw 'Cannot remove management service registration.' }
& sc.exe stop MemDisk
if ($LASTEXITCODE -notin @(0,1062)) { throw 'Cannot stop driver.' }
& sc.exe delete MemDisk
if ($LASTEXITCODE) { throw 'Cannot remove driver registration.' }
Write-Host 'Uninstalled. Test certificates and boot settings are unchanged; see README-VM.md for cleanup.'
