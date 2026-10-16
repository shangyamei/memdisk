#include "service.h"

namespace md { std::atomic<bool> Stopping = false; }
static SERVICE_STATUS_HANDLE statusHandle;
static SERVICE_STATUS status{};

static void Report(DWORD state, DWORD error = NO_ERROR) {
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP : 0;
    status.dwWin32ExitCode = error;
    status.dwWaitHint = state == SERVICE_STOP_PENDING ? 15000 : 0;
    SetServiceStatus(statusHandle, &status);
}
static void WINAPI Control(DWORD code) {
    if (code != SERVICE_CONTROL_STOP) return;
    const auto disks = md::DriverCall(md::Request(MdList));
    if (disks.Error || disks.Count) return; // Do not orphan active instances.
    Report(SERVICE_STOP_PENDING);
    md::Stopping = true;
    md::Handle wake(CreateFileW(MD_PIPE_PATH, GENERIC_READ | GENERIC_WRITE, 0,
        nullptr, OPEN_EXISTING, 0, nullptr));
}
static void WINAPI ServiceMain(DWORD, LPWSTR*) {
    statusHandle = RegisterServiceCtrlHandlerW(L"MemDiskService", Control);
    if (!statusHandle) return;
    Report(SERVICE_START_PENDING);
    Report(SERVICE_RUNNING);
    md::Serve();
    Report(SERVICE_STOPPED);
}
int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && wcscmp(argv[1], L"--console") == 0) { md::Serve(); return 0; }
    SERVICE_TABLE_ENTRYW table[] = {{const_cast<LPWSTR>(L"MemDiskService"), ServiceMain}, {nullptr,nullptr}};
    return StartServiceCtrlDispatcherW(table) ? 0 : static_cast<int>(GetLastError());
}
