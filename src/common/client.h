#pragma once
#include "memdisk/protocol.h"
#include <string>

namespace md {
class Handle {
public:
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h = INVALID_HANDLE_VALUE) : value(h) {}
    ~Handle() { if (value != INVALID_HANDLE_VALUE && value) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    explicit operator bool() const { return value && value != INVALID_HANDLE_VALUE; }
};
MD_REPLY DriverCall(const MD_REQUEST& request);
MD_REPLY ServiceCall(const MD_REQUEST& request,const WCHAR* pipePath=MD_PIPE_PATH);
MD_REQUEST Request(ULONG operation, ULONGLONG id = 0);
MD_REPLY Failure(DWORD error, const std::wstring& context);
std::wstring ErrorText(DWORD error);
bool ValidLabel(const WCHAR* label, size_t maxLength);
Handle RestrictedUserToken();
}
