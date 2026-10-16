$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
try {
    if (-not ([Security.Principal.WindowsPrincipal]::new($identity)).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Run as administrator.' }
} finally { $identity.Dispose() }
Add-Type -AssemblyName System.ServiceProcess
. (Join-Path $PSScriptRoot 'Uninstall-Helpers.ps1')
$cli = Join-Path $PSScriptRoot 'memdisk.exe'
Invoke-MdUninstallPlan -QueryDriver { Get-MdServiceState 'MemDisk' } `
    -QueryDisks { Read-MdDiskState $cli } -Remove { param($name) Remove-MdRegistration $name }
Write-Host 'Uninstalled. Test certificates and boot settings are unchanged; see README-VM.md for cleanup.'
