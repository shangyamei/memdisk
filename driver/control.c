#include "internal.h"

NTSTATUS MdControlRequest(PIRP irp, PIO_STACK_LOCATION stack)
{
    MD_REQUEST request;
    MD_REPLY* reply;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG i;
    if (stack->Parameters.DeviceIoControl.IoControlCode != MD_IOCTL_CONTROL)
        return MdComplete(irp, STATUS_INVALID_DEVICE_REQUEST, 0);
    if (stack->Parameters.DeviceIoControl.InputBufferLength != sizeof(MD_REQUEST) ||
        stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(MD_REPLY))
        return MdComplete(irp, STATUS_BUFFER_TOO_SMALL, 0);
    request = *(MD_REQUEST*)irp->AssociatedIrp.SystemBuffer;
    if (request.Version != MD_VERSION) return MdComplete(irp, STATUS_REVISION_MISMATCH, 0);
    reply = (MD_REPLY*)irp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(reply, sizeof(*reply));
    reply->Version = MD_VERSION;
    KeWaitForSingleObject(&MdManagerLock, Executive, KernelMode, FALSE, NULL);
    if (request.Operation == MdCreate) {
        status = MdCreateDisk(&request, reply);
    } else if (request.Operation == MdList) {
        for (i = 0; i < MD_MAX_DISKS; ++i) if (MdDisks[i]) {
            /* Aligned 64-bit counters are individually atomic on x64.
               This snapshot is not a transaction across all fields. */
            reply->Disks[reply->Count++] = MdDisks[i]->Info;
        }
    } else {
        for (i = 0; i < MD_MAX_DISKS; ++i)
            if (MdDisks[i] && MdDisks[i]->Info.Id == request.Id) break;
        if (i == MD_MAX_DISKS) status = STATUS_NOT_FOUND;
        else if (request.Operation == MdDestroy) {
            /* Direct clients must first remove the mount through the service. */
            if (MdDisks[i]->Info.Mounted) status = STATUS_DEVICE_BUSY;
            else { status = MdRemoveDisk(MdDisks[i]); if (NT_SUCCESS(status)) MdDisks[i] = NULL; }
        } else if (request.Operation == MdUnmount || request.Operation == MdMount) {
            status = MdSetMounted(MdDisks[i], request.Operation == MdMount);
        } else status = STATUS_INVALID_PARAMETER;
    }
    /* SCM cannot unload this legacy driver while any disk owns memory. */
    for (i = 0; i < MD_MAX_DISKS && !MdDisks[i]; ++i) {}
    MdDriver->DriverUnload = i == MD_MAX_DISKS ? MdUnload : NULL;
    KeReleaseMutex(&MdManagerLock, FALSE);
    reply->Error = (ULONG)status;
    return MdComplete(irp, STATUS_SUCCESS, sizeof(*reply));
}
