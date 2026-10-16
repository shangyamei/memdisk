# VM validation: no WDK or extra tools

## 1. Preparation

Use an isolated Windows 11 x64 VM with a snapshot, preferably >=4 GiB RAM.
Each creation is limited to 80% of currently available physical memory, in whole
MiB, with no fixed single-disk/aggregate capacity cap. Start small for smoke tests;
follow CAPACITY-UPDATE.md for larger-capacity and low-memory validation.
The host only builds; do not run the following installation on the build host.

The VM needs only its built-in Windows PowerShell 5.1, `bcdedit`, `certutil`,
`sc.exe` and filesystem utilities. All product binaries are supplied in dist.
An administrator terminal is required.

This driver has a self-signed development signature. Before the first load,
in the VM's elevated Command Prompt run:

```bat
bcdedit /set testsigning on
shutdown /r /t 0
```

If Secure Boot policy rejects this command, shut down the VM and disable Secure
Boot for this test VM in the hypervisor settings, then repeat. Do not change the
physical build machine's settings. If the guest uses BitLocker, preserve its
recovery key before changing VM firmware settings.

Do not install WDK, SDK, DevCon or a compiler in the VM. Do not silently disable
additional Windows protections if loading fails; collect CodeIntegrity events
first. HVCI / memory-integrity compatibility still requires actual verification.

Reference: https://learn.microsoft.com/en-us/windows-hardware/drivers/install/the-testsigning-boot-configuration-option

## 2. Copy and install

Copy `dist/MemDisk-0.1.8-dev-win-x64.zip` from the build machine to the VM.
In elevated built-in Windows PowerShell:

```powershell
Expand-Archive -LiteralPath C:\Users\Public\MemDisk-0.1.8-dev-win-x64.zip -DestinationPath C:\MemDisk
Set-Location C:\MemDisk
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Check-Package.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Install.ps1
```

Adapt the ZIP source path to its actual location. `Install.ps1` verifies file
hashes, imports only the supplied public development certificate into the VM's
machine Root/TrustedPublisher stores, and registers/starts the driver and service.
It does not enable test signing or restart the VM automatically.
Keep `C:\MemDisk` in place; SCM references the executables in this directory.

## 3. Automated smoke test

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Verify.ps1
```

Expected output includes PASS for both NTFS and exFAT and a final PASS line.
The script creates 64 MiB disks, writes deterministic data, validates SHA-256,
unmounts/remounts, verifies data again, and explicitly destroys only its own IDs.
Version 0.1.1 also uses a restricted token with the Administrators SID disabled
and privileges removed: ordinary capacity/file operations must succeed, while
raw data and management access must fail. This catches the gap in 0.1.0's
administrator-only smoke test. No extra guest software is required.
Version 0.1.2 also holds an actual test file open, checks that unmount safely
refuses at FSCTL_LOCK_VOLUME, releases the handle, and verifies normal unmount,
remount and destroy. Follow UPGRADE-0.1.2.md for old stuck instances.
If any step fails, stop testing and retain the console output and disk ID.

## 4. Manual UI and CLI checks

```powershell
.\memdisk.exe list --json
.\memdisk-ui.exe
```

In the GUI: create a 256 MiB NTFS disk, open its directory, write/copy files,
observe actual device throughput, switch light/dark, resize the window, and test
custom minimize/maximize/restore/close. Closing the GUI must keep the disk alive.
From a separate **non-elevated** PowerShell, replace R with the actual letter:

```powershell
.\memdisk-access-test.exe R
Get-ChildItem R:\
```

To manage a disk explicitly:

```powershell
.\memdisk.exe create --mib 256 --fs exfat --label Temp
.\memdisk.exe list
# Replace 1 with the actual returned ID, not the drive letter.
.\memdisk.exe unmount 1
.\memdisk.exe mount 1
.\memdisk.exe destroy 1 --yes
```

Hold a file open from another process and verify safe unmount refuses to proceed.
Release the handle before retrying. Do not run tests against physical disks.
For benchmark completion/cancellation and immediate detach regression, follow
UPGRADE-0.1.7.md. Host tests cannot replace these NTFS/exFAT checks.

## 5. Collect diagnostics on failure

```powershell
sc.exe query MemDisk
sc.exe query MemDiskService
.\memdisk.exe driver-list --json
Get-WinEvent -LogName Microsoft-Windows-CodeIntegrity/Operational -MaxEvents 20 |
    Format-List TimeCreated,Id,Message
Get-WinEvent -FilterHashtable @{LogName='System'; ProviderName='Service Control Manager'} -MaxEvents 20 |
    Format-List TimeCreated,Id,Message
Get-AuthenticodeSignature .\memdisk.sys | Format-List
```

Report the exact failed command, full output, Windows build, guest RAM,
Secure Boot/test-signing/memory-integrity state and any crash code/dump.
Do not keep retrying after a kernel crash; restore the snapshot for the next build.

## 6. Uninstall and restore VM settings

First list and explicitly destroy every test instance. Close all open files.

```powershell
.\memdisk.exe list
# Repeat for each returned ID:
.\memdisk.exe destroy 1 --yes
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Uninstall.ps1
```

The uninstall refuses to proceed if it cannot establish the active-disk state.
If the control device is absent, it also checks SCM to confirm the driver is
stopped or absent; this supports reboot and partial uninstall without starting
the driver. Follow UPGRADE-0.1.8.md for the regression matrix.
After uninstalling, find the exact supplied certificate thumbprint:

```powershell
$cert = [Security.Cryptography.X509Certificates.X509Certificate2]::new(
    (Join-Path $PWD 'MemDisk-Development.cer'))
$cert.Thumbprint
# Optional cleanup: remove only that exact test certificate.
certutil.exe -delstore Root $cert.Thumbprint
certutil.exe -delstore TrustedPublisher $cert.Thumbprint
bcdedit /set testsigning off
shutdown /r /t 0
```

Restore Secure Boot in the VM firmware if it was changed. Restoring the original
VM snapshot is also a complete cleanup option.
