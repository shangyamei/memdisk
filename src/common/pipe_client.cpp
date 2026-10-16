#include "client.h"

namespace md {
MD_REPLY ServiceCall(const MD_REQUEST& request,const WCHAR* pipePath) {
    const auto deadline = GetTickCount64() + 5000;
    Handle pipe;
    DWORD error = ERROR_FILE_NOT_FOUND;
    do {
        pipe.value = CreateFileW(pipePath, GENERIC_READ | GENERIC_WRITE, 0,
            nullptr, OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
        if (pipe) break;
        error = GetLastError();
        if (error != ERROR_PIPE_BUSY && error != ERROR_FILE_NOT_FOUND) break;
        // WaitNamedPipe returns immediately when no server instance exists.
        // Retry the gap between disconnect and a newly listening instance.
        if (error == ERROR_PIPE_BUSY) WaitNamedPipeW(pipePath, 50);
        else Sleep(20);
    } while (GetTickCount64() < deadline);
    if (!pipe) return Failure(error, L"Connect service");
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe.value, &mode, nullptr, nullptr))
        return Failure(GetLastError(), L"Pipe message mode");
    DWORD bytes = 0;
    if (!WriteFile(pipe.value, &request, sizeof(request), &bytes, nullptr) || bytes != sizeof(request))
        return Failure(GetLastError(), L"Send request");
    MD_REPLY reply{};
    if (!ReadFile(pipe.value, &reply, sizeof(reply), &bytes, nullptr))
        return Failure(GetLastError(), L"Receive reply");
    if (bytes != sizeof(reply) || reply.Version != MD_VERSION || reply.Count > MD_MAX_DISKS)
        return Failure(ERROR_INVALID_DATA, L"Service protocol");
    // Confirm receipt before the server disconnects its buffered message pipe.
    const ULONG ack = MD_VERSION;
    WriteFile(pipe.value, &ack, sizeof(ack), &bytes, nullptr);
    return reply;
}
}
