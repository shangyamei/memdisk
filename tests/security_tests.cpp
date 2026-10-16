#include "client.h"
#include "memdisk/security.h"
#include <sddl.h>
#include <stdexcept>

static bool Allowed(const WCHAR* sddl,HANDLE token,DWORD access) {
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    const auto complete=std::wstring(L"O:SYG:SY")+sddl;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(complete.c_str(),SDDL_REVISION_1,&descriptor,nullptr))
        throw std::runtime_error("Invalid security descriptor");
    GENERIC_MAPPING mapping{FILE_GENERIC_READ,FILE_GENERIC_WRITE,FILE_GENERIC_EXECUTE,FILE_ALL_ACCESS};
    MapGenericMask(&access,&mapping);
    // IoCreateDeviceSecure maps device SDDL generic ACEs. AccessCheck expects
    // an object-specific ACL, so reproduce that mapping in this user-mode test.
    PACL acl=nullptr;BOOL present=FALSE,defaulted=FALSE;
    if(GetSecurityDescriptorDacl(descriptor,&present,&acl,&defaulted)&&present&&acl){
        for(DWORD i=0;i<acl->AceCount;++i){void* raw=nullptr;
            if(GetAce(acl,i,&raw)){
                auto* ace=static_cast<ACCESS_ALLOWED_ACE*>(raw);
                if(ace->Header.AceType==ACCESS_ALLOWED_ACE_TYPE)MapGenericMask(&ace->Mask,&mapping);
            }
        }
    }
    alignas(PRIVILEGE_SET) unsigned char storage[1024]{};
    auto* privileges=reinterpret_cast<PRIVILEGE_SET*>(storage);
    DWORD length=sizeof(storage),granted=0;BOOL allowed=FALSE;
    const BOOL checked=AccessCheck(descriptor,token,access,&mapping,privileges,&length,&granted,&allowed);
    LocalFree(descriptor);
    if(!checked)throw std::runtime_error("AccessCheck failed");
    return allowed!=FALSE;
}
void SecurityTests(){
    auto token=md::RestrictedUserToken();
    if(!token)throw std::runtime_error("Cannot create ordinary-token test context");
    if(!Allowed(MD_DISK_SDDL,token.value,FILE_GENERIC_READ|FILE_GENERIC_WRITE|FILE_GENERIC_EXECUTE|DELETE))
        throw std::runtime_error("Ordinary disk traversal denied");
    if(Allowed(MD_DISK_SDDL,token.value,WRITE_DAC)||Allowed(MD_DISK_SDDL,token.value,WRITE_OWNER))
        throw std::runtime_error("Ordinary device security modification granted");
    if(!Allowed(MD_ROOT_SDDL,token.value,FILE_GENERIC_READ|FILE_GENERIC_WRITE|DELETE))
        throw std::runtime_error("Ordinary root file modification denied");
    if(Allowed(MD_ADMIN_SDDL,token.value,FILE_READ_DATA)||Allowed(MD_ADMIN_SDDL,token.value,FILE_WRITE_DATA)||
        Allowed(MD_ADMIN_SDDL,token.value,DELETE))
        throw std::runtime_error("Ordinary raw/management access was granted");
    if(Allowed(MD_ROOT_SDDL,token.value,WRITE_DAC))throw std::runtime_error("Ordinary root ACL modification granted");
}
