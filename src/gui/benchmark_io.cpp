#include "benchmark.h"
#include "detach.h"
#include <cstring>

namespace ui {
namespace {
BenchmarkResult Failure(const std::wstring& message){
    BenchmarkResult result;
    const int size=WideCharToMultiByte(CP_UTF8,0,message.c_str(),-1,nullptr,0,nullptr,nullptr);
    if(size>0){
        result.Message.resize(size);
        WideCharToMultiByte(CP_UTF8,0,message.c_str(),-1,result.Message.data(),size,nullptr,nullptr);
        result.Message.pop_back();
    }
    return result;
}
}
// Keep buffer and OVERLAPPED alive until cancellation has actually completed.
DWORD TransferBenchmarkBlock(HANDLE file,HANDLE event,void* data,DWORD size,ULONGLONG offset,
    bool read,std::stop_token stop){
    if(stop.stop_requested())return ERROR_OPERATION_ABORTED;
    if(!ResetEvent(event))return GetLastError();
    OVERLAPPED operation{};operation.hEvent=event;
    operation.Offset=static_cast<DWORD>(offset);operation.OffsetHigh=static_cast<DWORD>(offset>>32);
    DWORD bytes=0;
    const BOOL done=read?ReadFile(file,data,size,&bytes,&operation):WriteFile(file,data,size,&bytes,&operation);
    if(!done){
        const DWORD error=GetLastError();
        if(error!=ERROR_IO_PENDING)return error;
        while(WaitForSingleObject(event,50)==WAIT_TIMEOUT){
            if(stop.stop_requested()){
                CancelIoEx(file,&operation);
                GetOverlappedResult(file,&operation,&bytes,TRUE);
                return ERROR_OPERATION_ABORTED;
            }
        }
    }
    if(!GetOverlappedResult(file,&operation,&bytes,TRUE))return GetLastError();
    return bytes==size?ERROR_SUCCESS:ERROR_HANDLE_EOF;
}
BenchmarkResult RunBenchmark(const MD_DISK_INFO& disk,std::stop_token stop,const BenchmarkProgress& progress){
    constexpr DWORD block=1024*1024,blocks=16;
    if(stop.stop_requested())return Failure(L"测试已取消，资源已清理");
    {
        const auto path=std::wstring(L"\\\\.\\")+disk.Letter+L":";
        md::Handle volume(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,OPEN_EXISTING,0,nullptr));
        if(!volume)return Failure(L"打开目标卷失败："+md::ErrorText(GetLastError()));
        const DWORD error=md::VerifyIdentity(volume.value,disk.Id);
        if(error)return Failure(L"目标实例校验失败："+md::ErrorText(error));
    } // Close the identity-only volume handle before benchmark file I/O.
    const auto path=std::wstring(1,disk.Letter)+L":\\.memdisk-bench-"+
        std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".tmp";
    md::Handle file(CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_NO_BUFFERING|FILE_FLAG_WRITE_THROUGH|
        FILE_FLAG_DELETE_ON_CLOSE|FILE_FLAG_OVERLAPPED,nullptr));
    if(!file)return Failure(md::ErrorText(GetLastError()));
    md::Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));
    if(!event)return Failure(md::ErrorText(GetLastError()));
    auto* data=static_cast<unsigned char*>(VirtualAlloc(nullptr,block,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!data)return Failure(L"测试缓冲区分配失败");
    struct Free {void* P;~Free(){VirtualFree(P,0,MEM_RELEASE);}} release{data};
    LARGE_INTEGER frequency;
    if(!QueryPerformanceFrequency(&frequency)||!frequency.QuadPart)return Failure(L"性能计时器不可用");
    double speeds[2]{};
    for(int phase=0;phase<2;++phase){
        LONGLONG elapsed=0;
        for(DWORD i=0;i<blocks;++i){
            if(stop.stop_requested())return Failure(L"测试已取消，资源已清理");
            if(!phase)memset(data,static_cast<int>((i*17+23)&255),block);else memset(data,0,block);
            LARGE_INTEGER start,end;QueryPerformanceCounter(&start);
            const DWORD error=TransferBenchmarkBlock(file.value,event.value,data,block,ULONGLONG(i)*block,phase!=0,stop);
            QueryPerformanceCounter(&end);elapsed+=end.QuadPart-start.QuadPart;
            if(error)return Failure(error==ERROR_OPERATION_ABORTED?L"测试已取消，资源已清理":md::ErrorText(error));
            if(phase){
                const auto expected=static_cast<unsigned char>((i*17+23)&255);
                for(DWORD k=0;k<block;++k)if(data[k]!=expected)return Failure(L"数据校验失败");
            }
            progress((phase*blocks+i+1)/double(2*blocks));
        }
        speeds[phase]=elapsed?double(block)*blocks/MD_MIB*frequency.QuadPart/elapsed:0;
    }
    BenchmarkResult result;result.Progress=1;result.Read=speeds[1];result.Write=speeds[0];
    result.Message="完成 · 全量数据校验通过 · 测试文件已关闭清理";
    return result;
}
}
