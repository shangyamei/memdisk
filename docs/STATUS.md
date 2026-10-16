# Development build 0.1.6 — implementation and validation status

This is the first buildable VM validation milestone. It is not completion of the
entire SRS and is not a production release. Do not put valuable data on it.

## Implemented for VM verification

- Split WDM driver: allocation, lifetime, I/O, storage queries and volume queries.
- Entire nonpaged data storage allocated before publication; allocation rollback.
- Capacity admission: each creation may use up to 80% of currently available
  physical RAM. Fixed single-disk/aggregate caps and the 512 MiB reserve are
  removed; positive whole MiB and <=8 instances remain. Dynamic chunk metadata
  removes the former 1 GiB driver backing limit. See CAPACITY-UPDATE.md for
  component replacement and VM checks; larger-disk runtime validation is pending.
- Multiple fixed-size superfloppy-style RAM disks and global drive letters.
- Service orchestration of NTFS/exFAT formatting and lock/dismount operations.
- CLI create/list/mount/unmount/destroy with JSON output.
- Custom-chrome ImGui/ImPlot GUI, soft-grouped vertical layout, light/dark themes.
- Worker-thread telemetry, real device throughput, IOPS and approximate P99.
- Background filesystem space sampling and a real sequential read/write benchmark
  (16 MiB file, 1 MiB blocks, QD1, no buffering, write-through, full verification).
- Static user-mode runtime, file-based development signing, packaged public cert.
- Built-in PowerShell VM install, uninstall and filesystem integrity smoke test.

## Must be verified on the target VM

The build script supports VS 2022 and VS 2026, with automatic discovery or an
explicit `-VisualStudio` selection. VS 2026 requires CMake 4.2+; SDK/WDK remains
10.0.26100.0 by default, with an explicit `-WdkVersion` override available.
VS 2026 host compilation, CTest and packaging passed using SDK/WDK 28000;
see BUILD-REPORT.md. Toolchain support does not establish VM runtime compatibility.

The build host compiles and checks binaries only. Successful compilation does not
prove kernel safety or filesystem compatibility. The VM must establish:

1. Signed driver load under the chosen VM security configuration.
2. NTFS/exFAT formatting and filesystem mount compatibility of the legacy device.
3. Data correctness, dismount/remount, and complete instance cleanup.
4. GUI rendering on the VM GPU / WARP, DPI, resize and custom window behavior.
5. Driver Verifier, concurrency, low-memory and long-duration stress behavior.

`Verify.ps1` covers only basic NTFS/exFAT creation, 4 MiB deterministic data,
hash verification, remount and destroy. It intentionally leaves a failed disk
available for diagnosis and reports its ID instead of forcing destruction.

## SRS gaps retained for subsequent milestones

- Complete asynchronous operation progress, cancellation and event history.
- Complete system allocation accounting beyond the exposed metadata lower bound.
- Full benchmark engine, random/mixed workloads and data result export.
- Plot-linked cursors, selectable interval aggregation and CSV export.
- Latency P50/P95, independent read/write latency distributions and aggregation.
- All-disks aggregate view, column sorting/context menus and adjustable splitter.
- Full gap/partial-sample treatment and historical downsampling.
- Theme follow-system, settings persistence and complete accessibility coverage.
- Mount directories, automatic startup reconstruction, full installer rollback.
- Driver package INF/CAT workflow, production signing and release certification.
- Full SRS stability and performance acceptance campaign.

Version 0.1.1 allows authenticated-user filesystem traversal and initializes the
fresh NTFS root with inherited Modify access for authenticated users. Management
remains admin/SYSTEM-only. Raw disk data opens have an additional captured-token
admin check. A guest-side restricted-token access probe validates both intended
file access and denial of raw/management access; this needs VM runtime evidence.
This is a shared scratch volume policy, not per-user isolation; exFAT has no ACLs.

The reported VM smoke test of 0.1.0 passed NTFS/exFAT administrative operations.
Explorer subsequently showed Access Denied because normal-token access had not
been covered. See UPGRADE-0.1.1.md for the correction and retest procedure.

This package uses SCM legacy driver registration and an embedded test signature;
it does not install a PnP INF or require DevCon. The test certificate does not
make the driver a Microsoft production-signed release.

0.1.2 scopes SeManageVolumePrivilege to volume operations and reports the exact
detach stage. A direct recovery CLI shares the service's lock-before-dismount
pipeline and can handle old driver instances without replacing a running driver.
New VM tests retain an occupied volume, then verify detach after releasing it.
Actual 0.1.2 guest results remain to be collected.

0.1.3 updates GUI form contrast and replaces the disk table with selectable
capacity tiles (used/total, available space, percentage and progress). It reuses
the existing background space sampler and unchanged v1 management protocol.
An installed 0.1.2 service/driver can be retained while running the new GUI from
the new package directory. See UPGRADE-0.1.3.md; visual guest verification remains.

0.1.4 separates disk management (startup default) from monitoring/diagnostics.
Mount/unmount, destroy and directory actions are bound to each disk tile. The
monitoring page retains four sub-tabs and an explicit disk selector. Destruction
confirmation targets the originating instance ID, independently of selection.
See UPGRADE-0.1.4.md; this is a GUI-only change using the same service protocol.

0.1.5 reduces the footer to one font-height line, combining connection, sampling
and operation messages. Long messages are clipped with full text on hover.
