#include "internal.h"

NTSTATUS MdAllocateMemory(MD_DISK* disk)
{
    ULONGLONG remaining = disk->Info.Bytes;
    ULONGLONG count = remaining / MD_CHUNK_BYTES + (remaining % MD_CHUNK_BYTES != 0);
    SIZE_T tableBytes;
    if (!count || count > (MAXULONG_PTR - sizeof(MD_DISK)) / sizeof(PVOID))
        return STATUS_INVALID_PARAMETER;
    tableBytes = (SIZE_T)count * sizeof(PVOID);
    disk->Chunks = (PVOID*)ExAllocatePool2(POOL_FLAG_NON_PAGED, tableBytes, MD_TAG);
    if (!disk->Chunks) return STATUS_INSUFFICIENT_RESOURCES;
    /* Allocate virtual chunks, not a physically contiguous multi-GiB extent.
       Nothing is published until every chunk is owned and zeroed. */
    while (remaining) {
        SIZE_T size = (SIZE_T)MD_MIN(remaining, MD_CHUNK_BYTES);
        PVOID block = ExAllocatePool2(POOL_FLAG_NON_PAGED, size, MD_TAG);
        if (!block) { MdFreeMemory(disk); return STATUS_INSUFFICIENT_RESOURCES; }
        disk->Chunks[disk->ChunkCount++] = block; /* Zeroed by ExAllocatePool2. */
        remaining -= size;
    }
    disk->Info.MetadataBytes = sizeof(MD_DISK) + tableBytes;
    return STATUS_SUCCESS;
}

VOID MdFreeMemory(MD_DISK* disk)
{
    SIZE_T i;
    for (i = 0; i < disk->ChunkCount; ++i) {
        if (disk->Chunks[i]) ExFreePoolWithTag(disk->Chunks[i], MD_TAG);
        disk->Chunks[i] = NULL;
    }
    disk->ChunkCount = 0;
    if (disk->Chunks) ExFreePoolWithTag(disk->Chunks, MD_TAG);
    disk->Chunks = NULL;
}
