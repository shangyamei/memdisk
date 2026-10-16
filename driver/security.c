#include "internal.h"

/* Immutable admin-only descriptor used for raw data opens. Device namespace
   traversal is allowed to authenticated users; NTFS enforces file ACLs. */
static SECURITY_DESCRIPTOR MdRawSecurity;
static ULONG MdRawAclStorage[64];

NTSTATUS MdInitializeSecurity(VOID)
{
    PACL acl = (PACL)MdRawAclStorage;
    NTSTATUS status = RtlCreateSecurityDescriptor(&MdRawSecurity, SECURITY_DESCRIPTOR_REVISION);
    if (!NT_SUCCESS(status)) return status;
    status = RtlCreateAcl(acl, sizeof(MdRawAclStorage), ACL_REVISION);
    if (!NT_SUCCESS(status)) return status;
    status = RtlAddAccessAllowedAce(acl, ACL_REVISION, FILE_ALL_ACCESS, SeExports->SeLocalSystemSid);
    if (!NT_SUCCESS(status)) return status;
    status = RtlAddAccessAllowedAce(acl, ACL_REVISION, FILE_ALL_ACCESS, SeExports->SeAliasAdminsSid);
    if (!NT_SUCCESS(status)) return status;
    status = RtlSetOwnerSecurityDescriptor(&MdRawSecurity, SeExports->SeLocalSystemSid, FALSE);
    if (!NT_SUCCESS(status)) return status;
    status = RtlSetGroupSecurityDescriptor(&MdRawSecurity, SeExports->SeLocalSystemSid, FALSE);
    if (!NT_SUCCESS(status)) return status;
    return RtlSetDaclSecurityDescriptor(&MdRawSecurity, TRUE, acl, FALSE);
}

NTSTATUS MdAuthorizeDiskOpen(PIRP irp, PIO_STACK_LOCATION stack)
{
    PIO_SECURITY_CONTEXT context = stack->Parameters.Create.SecurityContext;
    ACCESS_MASK granted = 0, desired;
    PPRIVILEGE_SET privileges = NULL;
    NTSTATUS status = STATUS_ACCESS_DENIED;
    if (irp->RequestorMode == KernelMode) return STATUS_SUCCESS;
    if (!context || !context->AccessState || KeGetCurrentIrql() != PASSIVE_LEVEL)
        return STATUS_ACCESS_DENIED;
    /* This block driver has no file namespace. File opens belong to the FSD. */
    if (stack->FileObject && stack->FileObject->FileName.Length) return STATUS_OBJECT_NAME_NOT_FOUND;
    desired = context->DesiredAccess;
    if (!(desired & ~(FILE_READ_ATTRIBUTES | SYNCHRONIZE | READ_CONTROL))) return STATUS_SUCCESS;
    /* Use the request's captured context, including an impersonation / filtered
       token. Merely belonging to Administrators (deny-only) must not suffice. */
    if (SeAccessCheck(&MdRawSecurity, &context->AccessState->SubjectSecurityContext,
        FALSE, FILE_READ_DATA, 0, &privileges, IoGetFileObjectGenericMapping(),
        UserMode, &granted, &status)) status = STATUS_SUCCESS;
    if (privileges) SeFreePrivileges(privileges);
    return status;
}
