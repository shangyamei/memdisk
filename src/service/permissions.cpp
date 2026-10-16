#include "service.h"
#include "memdisk/security.h"
#include <aclapi.h>
#include <sddl.h>

namespace md {
DWORD ConfigureFileAccess(const MD_DISK_INFO& disk) {
    if (disk.FileSystem != MdNtfs) return ERROR_SUCCESS; // exFAT has no NTFS ACLs.
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(MD_ROOT_SDDL,
        SDDL_REVISION_1, &descriptor, nullptr)) return GetLastError();
    PACL acl = nullptr; BOOL present = FALSE, defaulted = FALSE;
    DWORD error = ERROR_INVALID_SECURITY_DESCR;
    if (GetSecurityDescriptorDacl(descriptor, &present, &acl, &defaulted) && present && acl) {
        WCHAR root[] = {disk.Letter,L':',L'\\',0};
        error = SetNamedSecurityInfoW(root, SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
            nullptr, nullptr, acl, nullptr);
    }
    LocalFree(descriptor);
    return error;
}
}
