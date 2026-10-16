#include "service.h"
#include <sddl.h>

namespace md {
static bool Transfer(HANDLE pipe, void* data, DWORD size, bool write, DWORD timeout) {
    Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event) return false;
    OVERLAPPED operation{}; operation.hEvent = event.value;
    DWORD bytes = 0;
    const BOOL done = write ? WriteFile(pipe, data, size, &bytes, &operation) :
        ReadFile(pipe, data, size, &bytes, &operation);
    if (!done && GetLastError() != ERROR_IO_PENDING) return false;
    if (!done && WaitForSingleObject(event.value, timeout) != WAIT_OBJECT_0) {
        CancelIoEx(pipe, &operation);
        GetOverlappedResult(pipe, &operation, &bytes, TRUE);
        return false;
    }
    return GetOverlappedResult(pipe, &operation, &bytes, TRUE) && bytes == size;
}
void Serve() {
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:P(A;;GA;;;SY)(A;;GA;;;BA)", SDDL_REVISION_1, &descriptor, nullptr)) return;
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE};
    // Keep one server instance alive across connections: no vanished-name gap.
    Handle pipe(CreateNamedPipeW(MD_PIPE_PATH, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1, sizeof(MD_REPLY), sizeof(MD_REQUEST), 5000, &attributes));
    if (!pipe) { LocalFree(descriptor); return; }
    while (!Stopping) {
        Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!event) break;
        OVERLAPPED connection{}; connection.hEvent = event.value;
        bool connected = ConnectNamedPipe(pipe.value, &connection) != FALSE;
        if (!connected) {
            const auto error = GetLastError();
            if (error == ERROR_PIPE_CONNECTED) connected = true;
            else if (error == ERROR_IO_PENDING) {
                while (!Stopping && WaitForSingleObject(event.value, 250) == WAIT_TIMEOUT) {}
                DWORD ignored = 0;
                if (Stopping) CancelIoEx(pipe.value, &connection);
                connected = GetOverlappedResult(pipe.value, &connection, &ignored, TRUE) != FALSE;
            }
        }
        if (!connected || Stopping) { DisconnectNamedPipe(pipe.value); continue; }
        MD_REQUEST request{};
        MD_REPLY reply = Failure(ERROR_INVALID_DATA, L"Request size");
        if (Transfer(pipe.value, &request, sizeof(request), false, 5000))
            reply = Execute(request);
        if (Transfer(pipe.value, &reply, sizeof(reply), true, 5000)) {
            ULONG ack = 0;
            Transfer(pipe.value, &ack, sizeof(ack), false, 5000);
        }
        // Receipt ACK + bounded waits prevent data loss without an unbounded flush.
        DisconnectNamedPipe(pipe.value);
    }
    LocalFree(descriptor);
}
}
