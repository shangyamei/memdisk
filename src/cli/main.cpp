#include "client.h"
#include "detach.h"
#include "memdisk/capacity.h"
#include "memdisk/json.h"
#include <iostream>
#include <string>

static void JsonString(const WCHAR* text) {
    md::WriteJsonString(std::wcout,text);
}
static void Print(const MD_REPLY& r, bool json) {
    if (json) {
        std::wcout << L"{\"version\":" << r.Version << L",\"error\":" << r.Error << L",\"message\":";
        JsonString(r.Message); std::wcout << L",\"disks\":[";
        for (ULONG i = 0; i < r.Count; ++i) {
            const auto& d = r.Disks[i]; if (i) std::wcout << L',';
            std::wcout << L"{\"id\":" << d.Id << L",\"bytes\":" << d.Bytes
                << L",\"letter\":\"" << d.Letter << L"\",\"filesystem\":\""
                << (d.FileSystem == MdNtfs ? L"NTFS" : L"exFAT") << L"\",\"mounted\":"
                << (d.Mounted ? L"true" : L"false") << L",\"readBytes\":" << d.Counters.ReadBytes
                << L",\"writeBytes\":" << d.Counters.WriteBytes << L",\"label\":";
            JsonString(d.Label); std::wcout << L'}';
        }
        std::wcout << L"]}\n";
    } else {
        if (r.Error) std::wcerr << L"Error " << r.Error << L": " << r.Message << L'\n';
        for (ULONG i = 0; i < r.Count; ++i) {
            const auto& d = r.Disks[i];
            std::wcout << d.Id << L"  " << d.Letter << L":  " << d.Label << L"  "
                << (d.FileSystem == MdNtfs ? L"NTFS" : L"exFAT") << L"  "
                << d.Bytes / MD_MIB << L" MiB  " << (d.Mounted ? L"mounted" : L"unmounted") << L'\n';
        }
    }
}
int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::wcout << L"MemDisk 0.1.8 (administrator)\n"
            L"  list [--json]\n  driver-list [--json]\n"
            L"  create --mib 256 --fs ntfs|exfat [--letter R] [--label Build] [--json]\n"
            L"  unmount ID | mount ID | destroy ID --yes [--json]\n"
            L"  recover-unmount ID | recover-destroy ID --yes [--json]\n";
        return 0;
    }
    bool json = false, yes = false, direct = false;
    auto request = md::Request(MdList);
    const std::wstring command = argv[1];
    if (command == L"driver-list") direct = true;
    else if (command == L"recover-unmount") {direct=true;request.Operation=MdUnmount;}
    else if (command == L"recover-destroy") {direct=true;request.Operation=MdDestroy;}
    else if (command == L"create") {
        request.Operation = MdCreate; request.Bytes = 256 * MD_MIB;
        request.FileSystem = MdNtfs; wcscpy_s(request.Label, L"MemDisk");
    } else if (command == L"mount") request.Operation = MdMount;
    else if (command == L"unmount") request.Operation = MdUnmount;
    else if (command == L"destroy") request.Operation = MdDestroy;
    else if (command != L"list") { std::wcerr << L"Unknown command\n"; return 2; }
    try {
        for (int i = 2; i < argc; ++i) {
            const std::wstring arg = argv[i];
            if (arg == L"--json") json = true;
            else if (arg == L"--yes") yes = true;
            else if (arg == L"--mib" && i+1 < argc) {
                const std::wstring value = argv[++i]; size_t used = 0;
                const auto mib = std::stoull(value, &used);
                if (value.empty() || value.find_first_not_of(L"0123456789") != std::wstring::npos ||
                    used != value.size() || mib > 0x7fffffffffffffffull / MD_MIB ||
                    !MdValidCapacity(mib * MD_MIB)) throw 1;
                request.Bytes = mib * MD_MIB;
            } else if (arg == L"--fs" && i+1 < argc) {
                const std::wstring fs = argv[++i];
                if (fs != L"ntfs" && fs != L"exfat") throw 1;
                request.FileSystem = fs == L"ntfs" ? MdNtfs : MdExfat;
            } else if (arg == L"--letter" && i+1 < argc) {
                const std::wstring letter = argv[++i];
                if (letter.size() != 1 || letter[0] < L'D' || letter[0] > L'Z') throw 1;
                request.Letter = letter[0];
            } else if (arg == L"--label" && i+1 < argc) {
                if (wcsncpy_s(request.Label, argv[++i], _TRUNCATE)) throw 1;
                if (!md::ValidLabel(request.Label, _countof(request.Label))) throw 1;
            } else if (request.Operation >= MdDestroy && i == 2) {
                size_t used = 0; request.Id = std::stoull(arg, &used);
                if (!request.Id || used != arg.size()) throw 1;
            } else throw 1;
        }
        if (request.Operation >= MdDestroy && !request.Id) throw 1;
        if (request.Operation == MdDestroy && !yes) {
            std::wcerr << L"Destructive operation: specify --yes to discard this RAM disk.\n"; return 2;
        }
    } catch (...) { std::wcerr << L"Invalid or missing argument. Run memdisk for usage.\n"; return 2; }
    const auto reply = direct ? (request.Operation==MdList?md::DriverCall(request):md::DetachDirect(request)) : md::ServiceCall(request);
    Print(reply, json);
    return reply.Error ? 1 : 0;
}
