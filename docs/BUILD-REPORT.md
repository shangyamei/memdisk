# Build verification — 2026-10-16

## File namespace deletion — 0.1.9 (2026-10-17)

- User reports ordinary-token Git clone failing to unlink .git/config.lock.
  Supplied icacls output shows inherited Modify for Authenticated Users on root,
  .git and config.lock; attrib shows Archive only, without Read-only.
- Code inspection found missing DELETE in the device namespace descriptor,
  despite FILE_DEVICE_SECURE_OPEN and a root ACL that permits Modify.
  Added namespace DELETE and changed the private security class GUID to avoid
  persisted defaults. Raw captured-token authorization and management ACLs remain.
- VS 2026 / SDK/WDK 28000 Release build, CTest (1/1), mocked uninstall tests,
  signing and packaging passed. Namespace DELETE and denial of WRITE_DAC/
  WRITE_OWNER are checked using an ordinary restricted token on the host.
- User confirmed on 2026-10-17 that the reported Git clone failure is fixed.
  This is user-reported evidence for the original ordinary-user scenario.
- Extended guest access probe covers nested config.lock rename/replace/delete
  and raw DELETE denial. The full NTFS/exFAT probe and kernel authorization matrix
  remain pending VM validation. Build verification did not load the driver or
  operate on R:. See UPGRADE-0.1.9.md.

## Uninstall state handling — 0.1.8 (2026-10-17)

- VS 2026 / SDK/WDK 28000 Release build, CTest (1/1), development signing,
  packaging and Windows PowerShell 5.1 package-hash checks passed.
- Pure mocked uninstall regressions pass under Windows PowerShell 5.1 and 7:
  stopped/missing/running-empty driver, active disks, access denial, running driver
  with missing control device, new disks during service removal, and stop failure.
  Malformed JSON/protocol responses are rejected.
- CLI JSON escaping covers non-ASCII errors, control characters, quotes, slashes
  and surrogate pairs. Read-only actual preflight in both PowerShell versions,
  with code page 936, returned SCM Missing, CLI exit 1/error 2, zero disks and passed.
- SCM adapters were only queried. No uninstall, service stop/deletion, driver
  loading, formatting or VM uninstall execution was performed on the build host.
  Full lifecycle validation remains in UPGRADE-0.1.8.md.

## Benchmark lifecycle fix — 0.1.7 (2026-10-17)

- VS 2026, SDK/WDK 28000, Release x64: all targets built, CTest passed (1/1),
  and the development-signed 0.1.7 ZIP was generated.
- Regression tasks hold cleanup open and verify Running remains true until
  resources are released, covering success, failure, exception and cancellation.
  The active instance rejects conflicting operation types; another ID is allowed.
- A real overlapped named-pipe read is cancelled and drained, then the pipe/event
  are reused successfully. No driver or filesystem volume is needed for the test.
- Initial CTest was blocked by transport tests using the live service pipe name.
  Tests now use a per-process test pipe; production calls retain their default
  endpoint. No running service was stopped and no existing disk was changed.
- Source line/whitespace checks pass. GUI/file-I/O changes require VM benchmark
  completion, stop and immediate safe-detach checks in UPGRADE-0.1.7.md.
- No driver installation/loading, formatting or VM runtime regression was run.
  These fixes do not prove the reported NTFS lock refusal's root cause.

## Capacity admission update — 0.1.6

- VS 2026, SDK/WDK 28000, Release x64: all targets compiled and CTest passed
  (1/1). Standard build generated the development-signed 0.1.6 ZIP.
- Capacity regression covers exact 80% arithmetic, rounding, maximum unsigned
  input without overflow, positive MiB alignment, signed-offset bounds, and
  capacities above the former single-disk and aggregate limits.
- CLI rejects zero, negative, fractional and overflowing capacities with exit 2
  before service access. Source line limits and git diff whitespace checks pass.
- Sandbox MSBuild FileTracker access denial required running the standard build
  outside the sandbox. Old 0.1.5 dist/memdisk.sys was occupied; the 0.1.6 package
  uses its own output directory.
- No driver load, disk creation or formatting occurred on this host. Dynamic
  chunk allocation/rollback and larger NTFS/exFAT disks require the isolated VM
  regressions in CAPACITY-UPDATE.md.

## VS 2026 build support — 0.1.5

- VS Community 2026 18.8, MSVC 19.51.36252.0, CMake 4.4.4, Release x64.
- This host has SDK/WDK 10.0.28000.0, not the default WDK 10.0.26100.0.
  Validation explicitly overrides WDK_VERSION; the project default is unchanged.
- Automatic and explicit VS 2026 discovery/configuration succeeded. All targets
  compiled, CTest passed (1/1), and the development-signed ZIP was generated.
- C++ targets explicitly enable /EHsc for exception unwinding; kernel C is unaffected.
- Packaging can discover installed SDK signing tools if SDK 26100 is absent.
- The build entry forwards Windows PowerShell 5.1 invocations to PowerShell 7
  with the original arguments. The redirected vswhere stream is explicitly
  decoded as UTF-8, including when PowerShell 7 inherits a legacy code page.
- Initial sandbox MSBuild FileTracker initialization failed with E_ACCESSDENIED;
  the complete build/test/package run succeeded outside the tool sandbox.
- VS 2022 remains supported by selection logic but is not installed on this host
  and was not rebuilt in this run. No VM runtime verification was performed.

Reproduce on this host:

```powershell
pwsh -NoProfile -File .\scripts\Build.ps1 -VisualStudio 2026 -WdkVersion 10.0.28000.0
```

The results below are the earlier 0.1.2 validation record.

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
