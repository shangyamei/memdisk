#include "service.h"
#include "memdisk/capacity.h"

namespace md {
static MD_REPLY Create(MD_REQUEST request) {
    if (!MdValidCapacity(request.Bytes) ||
        !ValidLabel(request.Label, _countof(request.Label)) ||
        (request.FileSystem != MdNtfs && request.FileSystem != MdExfat))
        return Failure(ERROR_INVALID_PARAMETER, L"Capacity: positive whole MiB; label: ASCII, <=11 characters");
    const DWORD drives = GetLogicalDrives();
    if (!request.Letter) for (WCHAR letter = L'R'; letter <= L'Z'; ++letter)
        if (!(drives & (1u << (letter-L'A')))) { request.Letter = letter; break; }
    if (request.Letter < L'D' || request.Letter > L'Z' || (drives & (1u << (request.Letter-L'A'))))
        return Failure(ERROR_ALREADY_ASSIGNED, L"Drive letter unavailable");
    MEMORYSTATUSEX memory{sizeof(memory)};
    if (!GlobalMemoryStatusEx(&memory)) return Failure(GetLastError(), L"Memory status");
    const auto budget = MdCapacityBudget(memory.ullAvailPhys);
    if (request.Bytes > budget)
        return Failure(ERROR_NOT_ENOUGH_MEMORY, L"Capacity exceeds 80% of available physical memory; maximum now: " +
            std::to_wstring(budget / MD_MIB) + L" MiB");
    auto reply = DriverCall(request);
    if (reply.Error) return reply;
    const auto disk = reply.Disks[0];
    DWORD error = Format(disk);
    if (!error) error = ConfigureFileAccess(disk);
    if (error) {
        const auto cleanup = SafeDetach(disk, true);
        return Failure(error, cleanup.Error ? std::wstring(L"Volume setup failed; rollback stopped at ")+
            DetachStage(cleanup.Step) : L"Volume setup failed; rolled back");
    }
    return DriverCall(Request(MdList));
}
MD_REPLY Execute(const MD_REQUEST& request) {
    if (request.Version != MD_VERSION) return Failure(ERROR_REVISION_MISMATCH, L"Protocol version");
    if (request.Operation == MdCreate) return Create(request);
    if (request.Operation == MdList) return DriverCall(request);
    if (request.Operation == MdUnmount || request.Operation == MdDestroy) return DetachDirect(request);
    auto disks = DriverCall(Request(MdList));
    if (disks.Error) return disks;
    const MD_DISK_INFO* disk = nullptr;
    for (ULONG i = 0; i < disks.Count; ++i) if (disks.Disks[i].Id == request.Id) disk = &disks.Disks[i];
    if (!disk) return Failure(ERROR_NOT_FOUND, L"Instance");
    if (request.Operation == MdMount) return DriverCall(request);
    return Failure(ERROR_INVALID_FUNCTION, L"Operation");
}
}
