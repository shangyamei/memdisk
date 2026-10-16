#include "client.h"

namespace md {
Handle RestrictedUserToken() {
    HANDLE processToken=nullptr,restricted=nullptr,impersonation=nullptr;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_DUPLICATE|TOKEN_QUERY,&processToken))return Handle();
    Handle process(processToken);
    DWORD sid[SECURITY_MAX_SID_SIZE/sizeof(DWORD)]{};DWORD size=sizeof(sid);
    if(!CreateWellKnownSid(WinBuiltinAdministratorsSid,nullptr,sid,&size))return Handle();
    SID_AND_ATTRIBUTES disabled{sid,0};
    if(!CreateRestrictedToken(process.value,DISABLE_MAX_PRIVILEGE,1,&disabled,0,nullptr,0,nullptr,&restricted))
        return Handle();
    Handle limited(restricted);
    if(!DuplicateTokenEx(limited.value,TOKEN_QUERY|TOKEN_IMPERSONATE,nullptr,
        SecurityImpersonation,TokenImpersonation,&impersonation))return Handle();
    return Handle(impersonation);
}
}
