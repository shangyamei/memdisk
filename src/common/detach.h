#pragma once
#include "client.h"
#include "memdisk/detach_plan.h"
namespace md {
DWORD VerifyIdentity(HANDLE volume,ULONGLONG id);
DetachResult SafeDetach(const MD_DISK_INFO& disk,bool destroy);
MD_REPLY DetachDirect(const MD_REQUEST& request);
}
