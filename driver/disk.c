#include "internal.h"

static volatile LONG64 MdNextId = 0;
/* New security-policy class: old IoCreateDeviceSecure defaults may be persisted. */
static const GUID MdDiskClass = {0x13d4fe79,0x233b,0x48bd,{0xae,0x29,0x09,0xda,0x41,0x8b,0xcc,0x62}};

NTSTATUS MdSetMounted(MD_DISK* disk, BOOLEAN mounted)
{
    UNICODE_STRING link, name;
    NTSTATUS status;
    if (disk->Info.Mounted == (ULONG)mounted) return STATUS_SUCCESS;
    RtlInitUnicodeString(&link, disk->LinkName);
    RtlInitUnicodeString(&name, disk->DeviceName);
    status = mounted ? IoCreateSymbolicLink(&link, &name) : IoDeleteSymbolicLink(&link);
    if (NT_SUCCESS(status)) disk->Info.Mounted = mounted;
    return status;
}

NTSTATUS MdCreateDisk(const MD_REQUEST* request, MD_REPLY* reply)
{
    ULONG slot, i;
    UNICODE_STRING name;
    /* Allow file namespace traversal; security.c separately gates raw opens. */
    UNICODE_STRING sddl = RTL_CONSTANT_STRING(MD_DISK_SDDL);
    WCHAR deviceName[64];
    PDEVICE_OBJECT device;
    MD_DISK* disk;
    NTSTATUS status;
    if (!MdValidCapacity(request->Bytes) ||
        request->Letter < L'D' || request->Letter > L'Z' ||
        (request->FileSystem != MdNtfs && request->FileSystem != MdExfat))
        return STATUS_INVALID_PARAMETER;
    for (slot = 0; slot < MD_MAX_DISKS && MdDisks[slot]; ++slot) {}
    if (slot == MD_MAX_DISKS) return STATUS_TOO_MANY_NAMES;
    for (i = 0; i < MD_MAX_DISKS; ++i) if (MdDisks[i]) {
        if (MdDisks[i]->Info.Letter == request->Letter) return STATUS_OBJECT_NAME_COLLISION;
    }
    RtlStringCchPrintfW(deviceName, RTL_NUMBER_OF(deviceName), L"\\Device\\MemDisk%llu",
        (ULONGLONG)InterlockedIncrement64(&MdNextId));
    RtlInitUnicodeString(&name, deviceName);
    status = IoCreateDeviceSecure(MdDriver, sizeof(MD_DISK), &name, FILE_DEVICE_DISK,
        FILE_DEVICE_SECURE_OPEN, FALSE, &sddl, &MdDiskClass, &device);
    if (!NT_SUCCESS(status)) return status;
    disk = (MD_DISK*)device->DeviceExtension;
    RtlZeroMemory(disk, sizeof(*disk));
    disk->Kind = MD_DISK_KIND;
    disk->Device = device;
    disk->Info.Id = (ULONGLONG)MdNextId;
    disk->Info.Bytes = request->Bytes;
    disk->Info.FileSystem = request->FileSystem;
    disk->Info.Letter = request->Letter;
    RtlCopyMemory(disk->Info.Label, request->Label, sizeof(disk->Info.Label));
    disk->Info.Label[RTL_NUMBER_OF(disk->Info.Label)-1] = 0;
    RtlStringCchCopyW(disk->DeviceName, RTL_NUMBER_OF(disk->DeviceName), deviceName);
    RtlStringCchPrintfW(disk->LinkName, RTL_NUMBER_OF(disk->LinkName),
        L"\\DosDevices\\Global\\%c:", request->Letter);
    KeInitializeSpinLock(&disk->DataLock);
    IoInitializeRemoveLock(&disk->RemoveLock, MD_TAG, 0, 0);
    status = MdAllocateMemory(disk);
    if (!NT_SUCCESS(status)) { IoDeleteDevice(device); return status; }
    device->Flags |= DO_DIRECT_IO;
    device->AlignmentRequirement = FILE_512_BYTE_ALIGNMENT;
    device->Flags &= ~DO_DEVICE_INITIALIZING;
    status = MdSetMounted(disk, TRUE);
    if (!NT_SUCCESS(status)) { MdFreeMemory(disk); IoDeleteDevice(device); return status; }
    MdDisks[slot] = disk;
    reply->Count = 1;
    reply->Disks[0] = disk->Info;
    return STATUS_SUCCESS;
}

NTSTATUS MdRemoveDisk(MD_DISK* disk)
{
    NTSTATUS status = IoAcquireRemoveLock(&disk->RemoveLock, disk);
    if (!NT_SUCCESS(status)) return status;
    InterlockedExchange(&disk->Removing, 1);
    MdSetMounted(disk, FALSE);
    IoReleaseRemoveLockAndWait(&disk->RemoveLock, disk);
    MdFreeMemory(disk);
    IoDeleteDevice(disk->Device);
    return STATUS_SUCCESS;
}
