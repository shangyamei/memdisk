#include "internal.h"

NTSTATUS MdCopyOutput(PIRP irp, PIO_STACK_LOCATION stack, const VOID* data, ULONG size, ULONG_PTR* bytes)
{
    if (stack->Parameters.DeviceIoControl.OutputBufferLength < size) return STATUS_BUFFER_TOO_SMALL;
    RtlCopyMemory(irp->AssociatedIrp.SystemBuffer, data, size);
    *bytes = size;
    return STATUS_SUCCESS;
}

static DISK_GEOMETRY MdGeometry(MD_DISK* disk)
{
    DISK_GEOMETRY geometry = {0};
    geometry.BytesPerSector = 512;
    geometry.SectorsPerTrack = 32;
    geometry.TracksPerCylinder = 64;
    geometry.Cylinders.QuadPart = disk->Info.Bytes / (512 * 32 * 64);
    geometry.MediaType = FixedMedia;
    return geometry;
}

static NTSTATUS MdProperty(PIRP irp, PIO_STACK_LOCATION stack, ULONG_PTR* bytes)
{
    STORAGE_PROPERTY_QUERY query;
    UCHAR buffer[256] = {0};
    ULONG size = 0, output = stack->Parameters.DeviceIoControl.OutputBufferLength;
    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(query)) return STATUS_INVALID_PARAMETER;
    query = *(STORAGE_PROPERTY_QUERY*)irp->AssociatedIrp.SystemBuffer;
    if (query.QueryType != PropertyStandardQuery) return STATUS_NOT_SUPPORTED;
    if (query.PropertyId == StorageDeviceProperty) {
        STORAGE_DEVICE_DESCRIPTOR* d = (STORAGE_DEVICE_DESCRIPTOR*)buffer;
        const CHAR product[] = "MemDisk RAM Disk";
        size = sizeof(*d) + sizeof(product);
        d->Version = sizeof(*d); d->Size = size;
        d->DeviceType = 0; d->BusType = BusTypeVirtual;
        d->ProductIdOffset = sizeof(*d);
        RtlCopyMemory(buffer + d->ProductIdOffset, product, sizeof(product));
    } else if (query.PropertyId == StorageAccessAlignmentProperty) {
        STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR* d = (STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR*)buffer;
        size = sizeof(*d); d->Version = size; d->Size = size;
        d->BytesPerLogicalSector = 512; d->BytesPerPhysicalSector = 512;
    } else if (query.PropertyId == StorageDeviceTrimProperty) {
        DEVICE_TRIM_DESCRIPTOR* d = (DEVICE_TRIM_DESCRIPTOR*)buffer;
        size = sizeof(*d); d->Version = size; d->Size = size; d->TrimEnabled = FALSE;
    } else if (query.PropertyId == StorageAdapterProperty) {
        STORAGE_ADAPTER_DESCRIPTOR* d = (STORAGE_ADAPTER_DESCRIPTOR*)buffer;
        size = sizeof(*d); d->Version = size; d->Size = size;
        d->MaximumTransferLength = 1024 * 1024;
        d->MaximumPhysicalPages = 257; d->AlignmentMask = 511;
        d->BusType = BusTypeVirtual;
    } else return STATUS_NOT_SUPPORTED;
    if (output < sizeof(STORAGE_DESCRIPTOR_HEADER)) return STATUS_BUFFER_TOO_SMALL;
    size = MD_MIN(size, output);
    return MdCopyOutput(irp, stack, buffer, size, bytes);
}

NTSTATUS MdStorageControl(MD_DISK* disk, PIRP irp, PIO_STACK_LOCATION stack, ULONG_PTR* bytes)
{
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    switch (code) {
    case MD_IOCTL_IDENTIFY:
        return MdCopyOutput(irp, stack, &disk->Info.Id, sizeof(disk->Info.Id), bytes);
    case IOCTL_DISK_GET_DRIVE_GEOMETRY: {
        DISK_GEOMETRY geometry = MdGeometry(disk);
        return MdCopyOutput(irp, stack, &geometry, sizeof(geometry), bytes);
    }
    case IOCTL_DISK_GET_DRIVE_GEOMETRY_EX: {
        DISK_GEOMETRY_EX geometry = {0};
        geometry.Geometry = MdGeometry(disk); geometry.DiskSize.QuadPart = disk->Info.Bytes;
        return MdCopyOutput(irp, stack, &geometry, sizeof(geometry), bytes);
    }
    case IOCTL_DISK_GET_LENGTH_INFO: {
        GET_LENGTH_INFORMATION length;
        length.Length.QuadPart = disk->Info.Bytes;
        return MdCopyOutput(irp, stack, &length, sizeof(length), bytes);
    }
    case IOCTL_DISK_GET_PARTITION_INFO: {
        PARTITION_INFORMATION partition = {0};
        partition.PartitionLength.QuadPart = disk->Info.Bytes;
        partition.PartitionNumber = 1; partition.PartitionType = PARTITION_IFS;
        partition.RecognizedPartition = TRUE;
        return MdCopyOutput(irp, stack, &partition, sizeof(partition), bytes);
    }
    case IOCTL_DISK_GET_PARTITION_INFO_EX: {
        PARTITION_INFORMATION_EX partition = {0};
        partition.PartitionStyle = PARTITION_STYLE_MBR;
        partition.PartitionLength.QuadPart = disk->Info.Bytes;
        partition.PartitionNumber = 1; partition.Mbr.PartitionType = PARTITION_IFS;
        partition.Mbr.RecognizedPartition = TRUE;
        return MdCopyOutput(irp, stack, &partition, sizeof(partition), bytes);
    }
    case IOCTL_DISK_GET_DRIVE_LAYOUT: {
        DRIVE_LAYOUT_INFORMATION layout = {0};
        layout.PartitionCount = 1; layout.Signature = (ULONG)disk->Info.Id;
        layout.PartitionEntry[0].PartitionLength.QuadPart = disk->Info.Bytes;
        layout.PartitionEntry[0].PartitionNumber = 1;
        layout.PartitionEntry[0].PartitionType = PARTITION_IFS;
        layout.PartitionEntry[0].RecognizedPartition = TRUE;
        return MdCopyOutput(irp, stack, &layout, sizeof(layout), bytes);
    }
    case IOCTL_DISK_GET_DRIVE_LAYOUT_EX: {
        DRIVE_LAYOUT_INFORMATION_EX layout = {0};
        layout.PartitionStyle = PARTITION_STYLE_MBR; layout.PartitionCount = 1;
        layout.Mbr.Signature = (ULONG)disk->Info.Id;
        layout.PartitionEntry[0].PartitionStyle = PARTITION_STYLE_MBR;
        layout.PartitionEntry[0].PartitionLength.QuadPart = disk->Info.Bytes;
        layout.PartitionEntry[0].PartitionNumber = 1;
        layout.PartitionEntry[0].Mbr.PartitionType = PARTITION_IFS;
        layout.PartitionEntry[0].Mbr.RecognizedPartition = TRUE;
        return MdCopyOutput(irp, stack, &layout, sizeof(layout), bytes);
    }
    case IOCTL_STORAGE_GET_DEVICE_NUMBER: {
        STORAGE_DEVICE_NUMBER number = {FILE_DEVICE_DISK, (ULONG)disk->Info.Id, 1};
        return MdCopyOutput(irp, stack, &number, sizeof(number), bytes);
    }
    case IOCTL_STORAGE_QUERY_PROPERTY: return MdProperty(irp, stack, bytes);
    case IOCTL_STORAGE_CHECK_VERIFY: case IOCTL_STORAGE_CHECK_VERIFY2:
    case IOCTL_DISK_CHECK_VERIFY:
        if (stack->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(ULONG)) {
            ULONG changes = 0; return MdCopyOutput(irp, stack, &changes, sizeof(changes), bytes);
        }
        return STATUS_SUCCESS;
    case IOCTL_DISK_IS_WRITABLE: return STATUS_SUCCESS;
    case IOCTL_STORAGE_GET_HOTPLUG_INFO: {
        STORAGE_HOTPLUG_INFO info = {0}; info.Size = sizeof(info);
        return MdCopyOutput(irp, stack, &info, sizeof(info), bytes);
    }
    case IOCTL_DISK_UPDATE_PROPERTIES: return STATUS_SUCCESS;
    case IOCTL_DISK_SET_PARTITION_INFO:
        if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(SET_PARTITION_INFORMATION))
            return STATUS_INVALID_PARAMETER;
        /* The superfloppy remains one partition; the formatter may set its type. */
        return STATUS_SUCCESS;
    default: return STATUS_INVALID_DEVICE_REQUEST;
    }
}
