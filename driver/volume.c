#include "internal.h"
#include <ntddvol.h>

static NTSTATUS MdMountName(MD_DISK* disk, PIRP irp, PIO_STACK_LOCATION stack,
    ULONG code, ULONG_PTR* bytes)
{
    UCHAR buffer[192] = {0};
    ULONG size;
    if (code == IOCTL_MOUNTDEV_QUERY_DEVICE_NAME) {
        MOUNTDEV_NAME* name = (MOUNTDEV_NAME*)buffer;
        name->NameLength = (USHORT)(wcslen(disk->DeviceName) * sizeof(WCHAR));
        RtlCopyMemory(name->Name, disk->DeviceName, name->NameLength);
        size = FIELD_OFFSET(MOUNTDEV_NAME, Name) + name->NameLength;
    } else {
        MOUNTDEV_UNIQUE_ID* id = (MOUNTDEV_UNIQUE_ID*)buffer;
        id->UniqueIdLength = sizeof(disk->Info.Id);
        RtlCopyMemory(id->UniqueId, &disk->Info.Id, sizeof(disk->Info.Id));
        size = FIELD_OFFSET(MOUNTDEV_UNIQUE_ID, UniqueId) + id->UniqueIdLength;
    }
    if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(USHORT)) return STATUS_BUFFER_TOO_SMALL;
    *bytes = MD_MIN(size, stack->Parameters.DeviceIoControl.OutputBufferLength);
    RtlCopyMemory(irp->AssociatedIrp.SystemBuffer, buffer, *bytes);
    return *bytes < size ? STATUS_BUFFER_OVERFLOW : STATUS_SUCCESS;
}

NTSTATUS MdVolumeControl(MD_DISK* disk, PIRP irp, PIO_STACK_LOCATION stack, ULONG_PTR* bytes)
{
    switch (stack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME: case IOCTL_MOUNTDEV_QUERY_UNIQUE_ID:
        return MdMountName(disk, irp, stack, stack->Parameters.DeviceIoControl.IoControlCode, bytes);
    case IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME:
        return STATUS_NOT_FOUND;
    case IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS: {
        VOLUME_DISK_EXTENTS extents = {0};
        extents.NumberOfDiskExtents = 1;
        extents.Extents[0].DiskNumber = (ULONG)disk->Info.Id;
        extents.Extents[0].ExtentLength.QuadPart = disk->Info.Bytes;
        return MdCopyOutput(irp, stack, &extents, sizeof(extents), bytes);
    }
    case IOCTL_VOLUME_ONLINE: case IOCTL_VOLUME_OFFLINE:
        return STATUS_SUCCESS;
    default: return STATUS_INVALID_DEVICE_REQUEST;
    }
}
