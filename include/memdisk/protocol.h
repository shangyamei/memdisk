#pragma once
/* Stable buffered ABI shared by kernel, service, CLI and GUI. No pointers on wire. */
#ifndef _KERNEL_MODE
#include <Windows.h>
#include <winioctl.h>
#endif
#define MD_VERSION 1u
#define MD_MAX_DISKS 8u
#define MD_LATENCY_BUCKETS 24u
#define MD_MIB (1024ull * 1024ull)
#define MD_CONTROL_PATH L"\\\\.\\MemDiskControl"
#define MD_PIPE_PATH L"\\\\.\\pipe\\MemDisk.v1"
#define MD_DEVICE_TYPE 0x8337u
#define MD_IOCTL_CONTROL CTL_CODE(MD_DEVICE_TYPE,0x800,METHOD_BUFFERED,FILE_READ_DATA|FILE_WRITE_DATA)
#define MD_IOCTL_IDENTIFY CTL_CODE(MD_DEVICE_TYPE,0x801,METHOD_BUFFERED,FILE_READ_DATA)
enum MD_OPERATION { MdList=1, MdCreate=2, MdDestroy=3, MdUnmount=4, MdMount=5 };
enum MD_FILESYSTEM { MdNtfs=1, MdExfat=2 };
typedef struct MD_REQUEST {
    ULONG Version;
    ULONG Operation;
    ULONGLONG Id;
    ULONGLONG Bytes;
    ULONG FileSystem;
    WCHAR Letter;
    WCHAR Label[32];
} MD_REQUEST;
typedef struct MD_COUNTERS {
    ULONGLONG ReadBytes;
    ULONGLONG WriteBytes;
    ULONGLONG Reads;
    ULONGLONG Writes;
    ULONGLONG Errors;
    ULONGLONG InFlight;
    ULONGLONG Latency[MD_LATENCY_BUCKETS];
} MD_COUNTERS;
typedef struct MD_DISK_INFO {
    ULONGLONG Id;
    ULONGLONG Bytes;
    ULONGLONG MetadataBytes;
    ULONG FileSystem;
    ULONG Mounted;
    WCHAR Letter;
    WCHAR Label[32];
    MD_COUNTERS Counters;
} MD_DISK_INFO;
typedef struct MD_REPLY {
    ULONG Version;
    ULONG Error; /* Driver: NTSTATUS; pipe service: Win32 error. */
    ULONG Count;
    ULONG Reserved;
    WCHAR Message[256];
    MD_DISK_INFO Disks[MD_MAX_DISKS];
} MD_REPLY;
