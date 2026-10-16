# Run from an elevated, built-in Windows PowerShell 5.1 session.
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not ([Security.Principal.WindowsPrincipal]::new($identity)).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Run as administrator.' }
& (Join-Path $PSScriptRoot 'Check-Package.ps1')
if (Get-Service -Name MemDisk,MemDiskService -ErrorAction SilentlyContinue) {
    throw 'Already installed. Destroy all RAM disks, run Uninstall.ps1, then install again.'
}
$certificate = Join-Path $PSScriptRoot 'MemDisk-Development.cer'
& certutil.exe -addstore -f Root $certificate
if ($LASTEXITCODE) { throw 'Test certificate root import failed.' }
& certutil.exe -addstore -f TrustedPublisher $certificate
if ($LASTEXITCODE) { throw 'Test publisher certificate import failed.' }
$driver = Join-Path $PSScriptRoot 'memdisk.sys'
$service = Join-Path $PSScriptRoot 'memdisk-service.exe'
& sc.exe create MemDisk type= kernel start= demand binPath= $driver
if ($LASTEXITCODE) { throw 'Driver service creation failed.' }
& sc.exe start MemDisk
if ($LASTEXITCODE) {
    & sc.exe delete MemDisk
    throw 'Driver start failed. Check TESTSIGNING, Secure Boot and CodeIntegrity event log; see README-VM.md.'
}
& sc.exe create MemDiskService type= own start= demand binPath= ('"' + $service + '"') depend= MemDisk
if ($LASTEXITCODE) { & sc.exe stop MemDisk; & sc.exe delete MemDisk; throw 'Service creation failed.' }
& sc.exe start MemDiskService
if ($LASTEXITCODE) { throw 'Service start failed. No RAM disk has been created.' }
Write-Host 'Installed. Keep this directory in place while services are registered.'
Write-Host 'Next: .\Verify.ps1 ; then .\memdisk-ui.exe'
