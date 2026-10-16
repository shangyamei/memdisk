#include "privilege.h"
namespace md {
ScopedPrivilege::ScopedPrivilege(const WCHAR* name) {
    if(!OpenThreadToken(GetCurrentThread(),TOKEN_QUERY|TOKEN_DUPLICATE|TOKEN_IMPERSONATE,
        TRUE,&previous_.value)) {
        error_=GetLastError();
        if(error_!=ERROR_NO_TOKEN)return;
        previous_.value=INVALID_HANDLE_VALUE;
    }
    Handle process;
    HANDLE source=previous_.value;
    if(!previous_) {
        if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY|TOKEN_DUPLICATE,&process.value)){
            error_=GetLastError();return;}
        source=process.value;
    }
    if(!DuplicateTokenEx(source,TOKEN_QUERY|TOKEN_ADJUST_PRIVILEGES|TOKEN_IMPERSONATE,
        nullptr,SecurityImpersonation,TokenImpersonation,&token_.value)){
        error_=GetLastError();return;}
    TOKEN_PRIVILEGES privileges{};privileges.PrivilegeCount=1;
    if(!LookupPrivilegeValueW(nullptr,name,&privileges.Privileges[0].Luid)){
        error_=GetLastError();return;}
    privileges.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;
    SetLastError(ERROR_SUCCESS);
    if(!AdjustTokenPrivileges(token_.value,FALSE,&privileges,0,nullptr,nullptr)){
        error_=GetLastError();return;}
    error_=GetLastError(); // TRUE + ERROR_NOT_ALL_ASSIGNED is still failure.
    if(error_!=ERROR_SUCCESS)return;
    if(!SetThreadToken(nullptr,token_.value)){error_=GetLastError();return;}
    installed_=true;
}
ScopedPrivilege::~ScopedPrivilege(){
    if(installed_){
        if(previous_) {if(!SetThreadToken(nullptr,previous_.value))RevertToSelf();}
        else RevertToSelf();
    }
}
}
