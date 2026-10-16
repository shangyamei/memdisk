# Build verification — 0.1.2 — 2026-10-16

## Build host results

- Windows x64, Visual Studio 2022 / MSVC 19.44.35228.0.
- Windows SDK/WDK 10.0.26100.0; Release configuration.
- Successfully compiled driver, service, CLI, native ImGui/ImPlot GUI and tests.
- Driver uses native subsystem, x64, GS entry/cookies, dynamic base and NX.
- Driver imports only Windows kernel components; user binaries import only
  Windows system libraries, with no external VC runtime DLL requirement.
- Shared C/C++ wire layout checked at compile time in both kernel and user mode.
- Host test suite passed: label validation, rates, percentile bounds, idle and
  counter-reset behavior, normal/invalid-version/truncated named-pipe exchanges.
- Added ordinary-token ACL policy checks and delayed pipe availability regression.
  These tests pass outside the tool sandbox; the restricted tool sandbox blocks
  creation of the test impersonation token. The build invokes tests in the
  unrestricted build process, with no driver installation or loading.
- Added detach failure injection: failure at any prerequisite prevents later
  dismount/removal/destruction. Privilege scope installation/restoration and
  invalid privilege errors pass host tests. Guest lock/occupancy tests are bundled.
- CLI invalid-capacity rejection checked without contacting the driver.
- All authored files passed the <=500-line gate; largest file is the SRS.
- All PowerShell scripts parsed successfully; packaged SHA-256 validation ran
  successfully under built-in Windows PowerShell 5.1.
- Embedded development signature applied using a private key kept outside dist.

The build host did not install or load the driver, format a volume, import the
test certificate, change boot options, or execute the VM validation script.

## Remaining runtime evidence

The user reported successful 0.1.0 VM driver loading, NTFS/exFAT formatting,
administrative file integrity and remount tests, followed by Explorer Access Denied.
The user subsequently reported generic Safe detach Access Denied in the GUI.
0.1.2 enables the volume privilege, reports exact failure stages and provides a
direct recovery path. The updated guest probes still require VM validation.
Concurrent I/O, stress, power-state behavior and complete GUI visual QA remain
outside the evidence collected so far.
Use `docs/VM-VALIDATION.md`; report the first failing command and full output.
No product performance or stability acceptance has been claimed.
