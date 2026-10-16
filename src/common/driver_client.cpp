#include "client.h"
#include <winternl.h>

namespace md {
MD_REQUEST Request(ULONG operation, ULONGLONG id) {
    MD_REQUEST r{}; r.Version = MD_VERSION; r.Operation = operation; r.Id = id; return r;
}
std::wstring ErrorText(DWORD error) {
    WCHAR text[512]{};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, text, 512, nullptr);
    return text[0] ? text : L"Error " + std::to_wstring(error);
}
MD_REPLY Failure(DWORD error, const std::wstring& context) {
    MD_REPLY reply{}; reply.Version = MD_VERSION; reply.Error = error;
    const auto message = context + L": " + ErrorText(error);
    wcsncpy_s(reply.Message, message.c_str(), _TRUNCATE);
    return reply;
}
bool ValidLabel(const WCHAR* label, size_t maxLength) {
    for (size_t i = 0; i < maxLength; ++i) {
        const WCHAR c = label[i];
        if (!c) return i <= 11; // exFAT-compatible common label subset.
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
            (c >= L'0' && c <= L'9') || c == L'_' || c == L'-')) return false;
    }
    return false;
}
MD_REPLY DriverCall(const MD_REQUEST& request) {
    Handle device(CreateFileW(MD_CONTROL_PATH, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
    if (!device) return Failure(GetLastError(), L"Open driver");
    MD_REPLY reply{}; DWORD bytes = 0;
    if (!DeviceIoControl(device.value, MD_IOCTL_CONTROL, const_cast<MD_REQUEST*>(&request),
        sizeof(request), &reply, sizeof(reply), &bytes, nullptr))
        return Failure(GetLastError(), L"Driver request");
    if (bytes != sizeof(reply) || reply.Version != MD_VERSION || reply.Count > MD_MAX_DISKS)
        return Failure(ERROR_INVALID_DATA, L"Driver protocol");
    if (reply.Error) {
        using Convert = ULONG(WINAPI*)(LONG);
        auto convert = reinterpret_cast<Convert>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlNtStatusToDosError"));
        return Failure(convert ? convert(static_cast<LONG>(reply.Error)) : ERROR_GEN_FAILURE, L"Driver");
    }
    return reply;
}
}
