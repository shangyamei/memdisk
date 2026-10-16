#include "internal.h"

static VOID MdRecord(MD_DISK* disk, LARGE_INTEGER begin, ULONG length, BOOLEAN write)
{
    ULONGLONG us = (ULONGLONG)(KeQueryPerformanceCounter(NULL).QuadPart - begin.QuadPart)
        * 1000000ull / (ULONGLONG)MdCounterFrequency.QuadPart;
    ULONG bucket = 0;
    while (us > 1 && bucket < MD_LATENCY_BUCKETS - 1) { us >>= 1; ++bucket; }
    InterlockedIncrement64((LONG64*)&disk->Info.Counters.Latency[bucket]);
    if (write) {
        InterlockedAdd64((LONG64*)&disk->Info.Counters.WriteBytes, length);
        InterlockedIncrement64((LONG64*)&disk->Info.Counters.Writes);
    } else {
        InterlockedAdd64((LONG64*)&disk->Info.Counters.ReadBytes, length);
        InterlockedIncrement64((LONG64*)&disk->Info.Counters.Reads);
    }
}

NTSTATUS MdTransfer(MD_DISK* disk, PIRP irp, PIO_STACK_LOCATION stack, ULONG_PTR* bytes)
{
    BOOLEAN write = stack->MajorFunction == IRP_MJ_WRITE;
    LONGLONG offset = stack->Parameters.Read.ByteOffset.QuadPart;
    ULONG length = stack->Parameters.Read.Length, done = 0;
    PUCHAR buffer;
    LARGE_INTEGER begin;
    if (offset < 0 || (ULONGLONG)offset > disk->Info.Bytes ||
        length > disk->Info.Bytes - (ULONGLONG)offset || (offset & 511) || (length & 511))
        return STATUS_INVALID_PARAMETER;
    if (!length) return STATUS_SUCCESS;
    if (!irp->MdlAddress || MmGetMdlByteCount(irp->MdlAddress) < length) return STATUS_INVALID_USER_BUFFER;
    buffer = (PUCHAR)MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority | MdlMappingNoExecute);
    if (!buffer) return STATUS_INSUFFICIENT_RESOURCES;
    begin = KeQueryPerformanceCounter(NULL);
    InterlockedIncrement64((LONG64*)&disk->Info.Counters.InFlight);
    /* Bounded critical sections: at most 64 KiB is copied under a spin lock.
       Request-wide atomicity is not promised; 512-byte sectors are protected. */
    while (done < length) {
        ULONGLONG pos = (ULONGLONG)offset + done;
        SIZE_T chunk = (SIZE_T)(pos / MD_CHUNK_BYTES);
        ULONG within = (ULONG)(pos % MD_CHUNK_BYTES);
        ULONG count = MD_MIN(length - done, MD_MIN(MD_CHUNK_BYTES - within, 64u * 1024u));
        KIRQL oldIrql;
        PUCHAR target = (PUCHAR)disk->Chunks[chunk] + within;
        KeAcquireSpinLock(&disk->DataLock, &oldIrql);
        if (write) RtlCopyMemory(target, buffer + done, count);
        else RtlCopyMemory(buffer + done, target, count);
        KeReleaseSpinLock(&disk->DataLock, oldIrql);
        done += count;
    }
    MdRecord(disk, begin, length, write);
    InterlockedDecrement64((LONG64*)&disk->Info.Counters.InFlight);
    *bytes = length;
    return STATUS_SUCCESS;
}

NTSTATUS MdDispatch(PDEVICE_OBJECT device, PIRP irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    MD_DISK* disk = (MD_DISK*)device->DeviceExtension;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR bytes = 0;
    if (disk->Kind == MD_CONTROL_KIND) {
        if (stack->MajorFunction == IRP_MJ_DEVICE_CONTROL) return MdControlRequest(irp, stack);
        if (stack->MajorFunction == IRP_MJ_CREATE || stack->MajorFunction == IRP_MJ_CLOSE ||
            stack->MajorFunction == IRP_MJ_CLEANUP) status = STATUS_SUCCESS;
        return MdComplete(irp, status, 0);
    }
    status = IoAcquireRemoveLock(&disk->RemoveLock, irp);
    if (!NT_SUCCESS(status)) return MdComplete(irp, status, 0);
    if (disk->Removing) status = STATUS_DELETE_PENDING;
    else switch (stack->MajorFunction) {
    case IRP_MJ_CREATE:
        status = MdAuthorizeDiskOpen(irp, stack); break;
    case IRP_MJ_CLOSE: case IRP_MJ_CLEANUP:
    case IRP_MJ_FLUSH_BUFFERS: case IRP_MJ_SHUTDOWN:
        status = STATUS_SUCCESS; break;
    case IRP_MJ_READ: case IRP_MJ_WRITE:
        status = MdTransfer(disk, irp, stack, &bytes); break;
    case IRP_MJ_DEVICE_CONTROL:
        status = MdStorageControl(disk, irp, stack, &bytes);
        if (status == STATUS_INVALID_DEVICE_REQUEST) status = MdVolumeControl(disk, irp, stack, &bytes);
        break;
    default: status = STATUS_INVALID_DEVICE_REQUEST; break;
    }
    /* Unsupported discovery IOCTLs are routine feature probes, not data failures. */
    if (!NT_SUCCESS(status) && (stack->MajorFunction == IRP_MJ_READ || stack->MajorFunction == IRP_MJ_WRITE))
        InterlockedIncrement64((LONG64*)&disk->Info.Counters.Errors);
    IoReleaseRemoveLock(&disk->RemoveLock, irp);
    return MdComplete(irp, status, bytes);
}
