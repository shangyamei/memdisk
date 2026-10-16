#include "service.h"
#include <vector>

namespace md {
static std::wstring VolumePath(WCHAR letter) { return std::wstring(L"\\\\.\\") + letter + L":"; }
DWORD Format(const MD_DISK_INFO& disk) {
    {
        Handle volume(CreateFileW(VolumePath(disk.Letter).c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
        if (!volume) return GetLastError();
        const DWORD error = VerifyIdentity(volume.value, disk.Id);
        if (error) return error;
    }
    WCHAR system[MAX_PATH]{}; GetSystemDirectoryW(system, MAX_PATH);
    const std::wstring executable = std::wstring(system) + L"\\format.com";
    // No shell. Letter, FS and label are validated and not arbitrary command text.
    std::wstring command = L"\"" + executable + L"\" " + disk.Letter + L": /Q /Y /FS:" +
        (disk.FileSystem == MdNtfs ? L"NTFS" : L"exFAT") + L" /V:" + disk.Label;
    STARTUPINFOW start{sizeof(start)}; PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &start, &process)) return GetLastError();
    Handle thread(process.hThread), child(process.hProcess);
    if (WaitForSingleObject(child.value, 120000) != WAIT_OBJECT_0) {
        TerminateProcess(child.value, ERROR_TIMEOUT);
        WaitForSingleObject(child.value, 5000);
        return ERROR_TIMEOUT;
    }
    DWORD code = ERROR_GEN_FAILURE; GetExitCodeProcess(child.value, &code);
    return code ? ERROR_UNRECOGNIZED_VOLUME : ERROR_SUCCESS;
}
}
