#include "internal.h"

NTSTATUS MdAllocateMemory(MD_DISK* disk)
{
    ULONGLONG remaining = disk->Info.Bytes;
    ULONG i = 0;
    /* Allocate virtual chunks, not a physically contiguous multi-GiB extent.
       Nothing is published until every chunk is owned and zeroed. */
    while (remaining) {
        SIZE_T size = (SIZE_T)MD_MIN(remaining, MD_CHUNK_BYTES);
        PVOID block = ExAllocatePool2(POOL_FLAG_NON_PAGED, size, MD_TAG);
        if (!block) { MdFreeMemory(disk); return STATUS_INSUFFICIENT_RESOURCES; }
        disk->Chunks[i++] = block; /* ExAllocatePool2 zeroes by default. */
        disk->ChunkCount = i;
        remaining -= size;
    }
    disk->Info.MetadataBytes = sizeof(MD_DISK);
    return STATUS_SUCCESS;
}

VOID MdFreeMemory(MD_DISK* disk)
{
    ULONG i;
    for (i = 0; i < disk->ChunkCount; ++i) {
        if (disk->Chunks[i]) ExFreePoolWithTag(disk->Chunks[i], MD_TAG);
        disk->Chunks[i] = NULL;
    }
    disk->ChunkCount = 0;
}
