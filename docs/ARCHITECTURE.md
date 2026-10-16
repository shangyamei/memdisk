# Architecture and learning path

## Separation of concerns

```text
GUI / CLI -- local admin-only named pipe --> service
service -- versioned buffered IOCTL --> control device
applications -- Windows NTFS/exFAT --> virtual disk read/write dispatch
```

The first driver is a demand-loaded legacy WDM virtual disk driver, not a
Storport miniport or root-enumerated PnP disk. It exposes superfloppy-style
volumes (one whole-device filesystem) and globally visible DOS drive links.
This minimizes dependencies for the first VM validation but its format/mount
compatibility must be proven on the actual target OS. It does not claim full
physical disk / Disk Management / Storage Spaces compatibility.

## Driver: read in this order

| File | Responsibility | Learning focus |
| --- | --- | --- |
| `include/memdisk/protocol.h` | Fixed-size wire structures | Shared ABI, versioning, no user pointers |
| `driver/internal.h` | Private disk state and contracts | Ownership, lifetime, synchronization |
| `driver/entry.c` | Entry, secure control device, completion | Driver object and dispatch |
| `driver/control.c` | Management request validation and routing | Control plane, serialized mutation |
| `driver/memory.c` | Full nonpaged allocation and rollback | Resource ownership, physical vs virtual continuity |
| `driver/disk.c` | Device creation, mount link, removal | Publish only after allocation, remove locks |
| `driver/io.c` | Read/write and counters | MDL mapping, bounds, bounded spin sections |
| `driver/storage.c` | Geometry and storage properties | Protocol compatibility, truthful capabilities |
| `driver/volume.c` | Mount device and volume queries | Device identity and filesystem integration |

Manager operations use a mutex at PASSIVE_LEVEL, because creating devices and
namespace links must not occur under a raised-IRQL fast mutex. Disk requests use
remove locks; backing memory is freed only after in-flight dispatches drain.
Copies are protected in <=64 KiB sections; multi-sector request atomicity is not
promised. Writes complete synchronously and there is no internal write-back cache.

Data is allocated in <=16 MiB nonpaged pool chunks using `ExAllocatePool2`.
All chunks are allocated and zero-initialized before the disk is linked.
Allocation failure frees earlier chunks. Chunking does not implement sparse
allocation or deferred allocation. A chunk need not be physically contiguous.

The driver disallows unload while any instance remains. The service additionally
requires successful filesystem lock and dismount before removing a mount or
destroying an instance. No force-removal command is provided.

## User mode

| Area | Responsibility |
| --- | --- |
| `src/common` | RAII handles, transport, errors, protocol validation |
| `src/service/main.cpp` | SCM lifecycle |
| `src/service/server.cpp` | Admin/SYSTEM-only local pipe |
| `src/service/operations.cpp` | Admission and operation orchestration |
| `src/service/volume.cpp` | Verify instance identity, format, safe dismount |
| `src/cli` | Parameter validation and human/JSON output |
| `src/gui/window.cpp` | Win32 custom chrome, sizing, D3D11 |
| `src/gui/model.cpp` | Worker thread, snapshots, rate/percentile calculation |
| `src/gui/view.cpp` | Soft-grouped vertical interface and commands |
| `src/gui/panels.cpp` | Plots and details |
| `src/gui/benchmark.cpp` | Worker-thread file benchmark, data validation, cleanup |

## Current limits

Development defaults are 64–1024 MiB per disk, <=8 disks, <=4 GiB total. Service
admission additionally requires the requested disk to be <=25% of physical RAM
and at least 512 MiB currently available after allocation. These are initial
validation limits, not finalized product specifications or a guarantee against
concurrent allocations by other software.

The current service serializes management and polling. Long formatting can
temporarily interrupt telemetry, but it never forwards application file I/O.
Future revisions should separate long-running operations from status queries.
