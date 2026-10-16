#pragma once
#include "client.h"
#include "detach.h"
#include <atomic>

namespace md {
extern std::atomic<bool> Stopping;
void Serve();
MD_REPLY Execute(const MD_REQUEST& request);
DWORD Format(const MD_DISK_INFO& disk);
DWORD ConfigureFileAccess(const MD_DISK_INFO& disk);
}
