# MemDisk

Windows x64 RAM disk development project. Entire disk data storage is allocated
from nonpaged memory before publishing the drive. NTFS/exFAT are supplied by Windows.

**Status: development validation build, not production-certified.** Read
[implementation status](docs/STATUS.md) before testing. The build host never loads
the driver; kernel execution and formatting are reserved for your isolated VM.

## Build on the development machine

Requires Visual Studio 2022 C++ x64 tools, Windows SDK/WDK 10.0.26100.0,
CMake 3.24+, Git and PowerShell 7. First configure downloads pinned ImGui/ImPlot
source commits. Later builds can reuse `out/build/_deps` offline.

```powershell
pwsh -NoProfile -File .\scripts\Build.ps1
```

Output: `dist/MemDisk-0.1.5-dev-win-x64.zip`.
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
