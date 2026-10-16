#include "internal.h"

PDRIVER_OBJECT MdDriver;
PDEVICE_OBJECT MdControl;
KMUTEX MdManagerLock;
MD_DISK* MdDisks[MD_MAX_DISKS];
LARGE_INTEGER MdCounterFrequency;
/* Private class GUID. Only SYSTEM and administrators may manage disks. */
static const GUID MdClass = {0x9aa22025,0x9e34,0x4015,{0x91,0x61,0x6a,0xcc,0x8d,0x4c,0x01,0x77}};

NTSTATUS MdComplete(PIRP irp, NTSTATUS status, ULONG_PTR bytes)
{
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = NT_SUCCESS(status) || status == STATUS_BUFFER_OVERFLOW ? bytes : 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

VOID MdUnload(PDRIVER_OBJECT driver)
{
    UNICODE_STRING link = RTL_CONSTANT_STRING(L"\\DosDevices\\MemDiskControl");
    ULONG i;
    UNREFERENCED_PARAMETER(driver);
    /* The management service refuses stop/uninstall while an instance exists. */
    for (i = 0; i < MD_MAX_DISKS; ++i) {
        if (MdDisks[i]) {
            MdRemoveDisk(MdDisks[i]);
            MdDisks[i] = NULL;
        }
    }
    IoDeleteSymbolicLink(&link);
    if (MdControl) IoDeleteDevice(MdControl);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
    UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\Device\\MemDiskControl");
    UNICODE_STRING link = RTL_CONSTANT_STRING(L"\\DosDevices\\MemDiskControl");
    UNICODE_STRING sddl = RTL_CONSTANT_STRING(MD_ADMIN_SDDL);
    NTSTATUS status;
    ULONG i;
    UNREFERENCED_PARAMETER(registry);
    MdDriver = driver;
    status = MdInitializeSecurity();
    if (!NT_SUCCESS(status)) return status;
    KeInitializeMutex(&MdManagerLock, 0);
    KeQueryPerformanceCounter(&MdCounterFrequency);
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i) driver->MajorFunction[i] = MdDispatch;
    driver->DriverUnload = MdUnload;
    status = IoCreateDeviceSecure(driver, sizeof(ULONG), &name, MD_DEVICE_TYPE,
        FILE_DEVICE_SECURE_OPEN, FALSE, &sddl, &MdClass, &MdControl);
    if (!NT_SUCCESS(status)) return status;
    *(ULONG*)MdControl->DeviceExtension = MD_CONTROL_KIND;
    status = IoCreateSymbolicLink(&link, &name);
    if (!NT_SUCCESS(status)) { IoDeleteDevice(MdControl); MdControl = NULL; return status; }
    MdControl->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}
