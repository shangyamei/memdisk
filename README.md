# MemDisk

Windows x64 RAM disk development project. Entire disk data storage is allocated
from nonpaged memory before publishing the drive. NTFS/exFAT are supplied by Windows.

**Status: development validation build, not production-certified.** Read
[implementation status](docs/STATUS.md) before testing. The build host never loads
the driver; kernel execution and formatting are reserved for your isolated VM.

## Build on the development machine

Requires Visual Studio 2022 or 2026 C++ x64 tools, Windows SDK/WDK 10.0.26100.0,
CMake 3.24+ (4.2+ for VS 2026), Git and PowerShell 7. First configure downloads pinned ImGui/ImPlot
source commits. Later builds can reuse `out/build/_deps` offline.

```powershell
pwsh -NoProfile -File .\scripts\Build.ps1
```

The script selects the newest installed supported VS with C++ x64 tools.
When invoked from Windows PowerShell 5.1, it forwards the same arguments to
installed PowerShell 7. Visual Studio discovery explicitly decodes UTF-8 output
independently of the terminal code page.
Use `-VisualStudio 2022` or `-VisualStudio 2026` to select a specific version.
Switching VS refreshes CMake metadata in `out/build`, retaining `_deps` downloads;
custom cached CMake options must then be reapplied. Direct CMake users can use
`-G "Visual Studio 18 2026" -A x64` with CMake 4.2+.

The default driver WDK remains 10.0.26100.0. To build with another installed WDK,
pass e.g. `-VisualStudio 2026 -WdkVersion 10.0.28000.0`; retest the resulting
driver in the isolated VM. Packaging prefers the SDK 26100 x64 signing tool,
falling back to the newest installed Windows 10 SDK x64 signing tool.

Output: `dist/MemDisk-0.1.9-dev-win-x64.zip`.
Disk capacity has no fixed single-disk or aggregate cap: each creation may request
up to 80% of currently available physical RAM, in whole MiB (maximum 8 instances).
Allocation and filesystem formatting can still fail. Replace the driver, service,
CLI and GUI together; see [capacity update](docs/CAPACITY-UPDATE.md).
Version 0.1.7 fixes benchmark handle cleanup, cancellation and conflicting GUI
operations; see [upgrade and regression steps](docs/UPGRADE-0.1.7.md).
Version 0.1.8 supports uninstall after reboot or partial removal; the script
cross-checks driver registration/state and the control device, retaining active
disk protection. See [uninstall recovery](docs/UPGRADE-0.1.8.md).
Version 0.1.9 adds device namespace delete access for ordinary file rename/delete
operations, while keeping raw disk and management access restricted. This requires
replacing the driver; see [file access update](docs/UPGRADE-0.1.9.md).
All user-mode binaries statically link the MSVC runtime. The VM needs no compiler,
WDK, SDK, Visual Studio, Git, Python, Node.js or VC redistributable installation.
Windows PowerShell 5.1 and built-in Windows commands are sufficient.

## Validate on the VM

Copy the ZIP to the VM, extract to `C:\MemDisk`, then follow
[VM validation guide](docs/VM-VALIDATION.md). Test signing must be enabled on the
VM; the package supplies a development-signed driver and its public certificate.
No boot/security settings or certificate stores are changed on the build host.

## Read the code

- [Architecture and learning path](docs/ARCHITECTURE.md)
- [SRS draft](docs/SRS.md)
- [VM validation](docs/VM-VALIDATION.md)
- [Known gaps](docs/STATUS.md)

Each authored file must stay at or below 500 lines. `Check-Source.ps1` enforces
this before every build. Unmodified upstream ImGui/ImPlot sources and generated
build output live outside the authored tree, in ignored `out/`.

Source licensing for this project is not yet selected. Bundled ImGui and ImPlot
license notices are included in `dist/licenses/`.
