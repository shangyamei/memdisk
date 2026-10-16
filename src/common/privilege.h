#pragma once
#include "client.h"
namespace md {
// Private thread token: never enable a privilege globally on the service.
class ScopedPrivilege {
    Handle token_,previous_;
    bool installed_=false;
    DWORD error_=ERROR_SUCCESS;
public:
    explicit ScopedPrivilege(const WCHAR* name);
    ~ScopedPrivilege();
    ScopedPrivilege(const ScopedPrivilege&)=delete;
    ScopedPrivilege& operator=(const ScopedPrivilege&)=delete;
    DWORD Error() const { return error_; }
};
}
