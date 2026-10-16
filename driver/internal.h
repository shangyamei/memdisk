#pragma once
#include <ntifs.h>
#include <ntdddisk.h>
#include <ntddstor.h>
#include <mountdev.h>
#include <ntstrsafe.h>
#include <wdmsec.h>
#include "memdisk/protocol.h"
#include "memdisk/security.h"

#define MD_TAG 'kDsM'
#define MD_MIN(a,b) (((a)<(b))?(a):(b))
#define MD_CHUNK_BYTES (16u * 1024u * 1024u)
#define MD_MAX_CHUNKS 64u
#define MD_CONTROL_KIND 1u
#define MD_DISK_KIND 2u
C_ASSERT(sizeof(MD_REQUEST) == 96);
C_ASSERT(sizeof(MD_REPLY) == 3280);
typedef struct MD_DISK {
    ULONG Kind; /* First word must match the control extension. */
    IO_REMOVE_LOCK RemoveLock;
    KSPIN_LOCK DataLock;
    volatile LONG Removing;
    PDEVICE_OBJECT Device;
    MD_DISK_INFO Info;
    ULONG ChunkCount;
    PVOID Chunks[MD_MAX_CHUNKS];
    WCHAR DeviceName[64];
    WCHAR LinkName[32];
} MD_DISK;
extern PDRIVER_OBJECT MdDriver;
extern PDEVICE_OBJECT MdControl;
extern KMUTEX MdManagerLock;
extern MD_DISK* MdDisks[MD_MAX_DISKS];
extern LARGE_INTEGER MdCounterFrequency;

NTSTATUS MdComplete(PIRP irp, NTSTATUS status, ULONG_PTR bytes);
DRIVER_DISPATCH MdDispatch;
DRIVER_UNLOAD MdUnload;
NTSTATUS MdControlRequest(PIRP irp, PIO_STACK_LOCATION stack);
NTSTATUS MdCreateDisk(const MD_REQUEST* request, MD_REPLY* reply);
NTSTATUS MdRemoveDisk(MD_DISK* disk);
NTSTATUS MdSetMounted(MD_DISK* disk, BOOLEAN mounted);
NTSTATUS MdAllocateMemory(MD_DISK* disk);
VOID MdFreeMemory(MD_DISK* disk);
NTSTATUS MdTransfer(MD_DISK* disk, PIRP irp, PIO_STACK_LOCATION stack, ULONG_PTR* bytes);
NTSTATUS MdStorageControl(MD_DISK* disk, PIRP irp, PIO_STACK_LOCATION stack, ULONG_PTR* bytes);
NTSTATUS MdVolumeControl(MD_DISK* disk, PIRP irp, PIO_STACK_LOCATION stack, ULONG_PTR* bytes);
NTSTATUS MdCopyOutput(PIRP irp, PIO_STACK_LOCATION stack, const VOID* data, ULONG size, ULONG_PTR* bytes);
NTSTATUS MdInitializeSecurity(VOID);
NTSTATUS MdAuthorizeDiskOpen(PIRP irp, PIO_STACK_LOCATION stack);
