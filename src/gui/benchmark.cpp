#include "app.h"
#include <chrono>
#include <cstring>

namespace ui {
namespace {
struct Result {
    bool Running=false;
    double Progress=0,Read=0,Write=0;
    std::string Message="等待测试";
    ULONGLONG Id=0;
};
class Runner {
    std::mutex mutex_;
    Result result_;
    std::jthread worker_;
    void Fail(const std::wstring& text){std::lock_guard lock(mutex_);result_.Running=false;result_.Message=Utf8(text);}
    void Run(MD_DISK_INFO disk,std::stop_token stop){
        constexpr DWORD block=1024*1024,blocks=16;
        WCHAR volumePath[8]={L'\\',L'\\',L'.',L'\\',disk.Letter,L':',0};
        md::Handle volume(CreateFileW(volumePath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,OPEN_EXISTING,0,nullptr));
        ULONGLONG id=0;DWORD bytes=0;
        if(!volume||!DeviceIoControl(volume.value,MD_IOCTL_IDENTIFY,nullptr,0,&id,sizeof(id),&bytes,nullptr)||id!=disk.Id){
            Fail(L"目标实例校验失败");return;}
        const auto path=std::wstring(1,disk.Letter)+L":\\.memdisk-bench-"+
            std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".tmp";
        md::Handle file(CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_NO_BUFFERING|FILE_FLAG_WRITE_THROUGH|FILE_FLAG_DELETE_ON_CLOSE,nullptr));
        if(!file){Fail(md::ErrorText(GetLastError()));return;}
        auto* data=static_cast<unsigned char*>(VirtualAlloc(nullptr,block,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        if(!data){Fail(L"测试缓冲区分配失败");return;}
        struct Free {void* P;~Free(){VirtualFree(P,0,MEM_RELEASE);}} release{data};
        LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
        double speeds[2]{};
        for(int phase=0;phase<2;++phase){
            LARGE_INTEGER zero{};SetFilePointerEx(file.value,zero,nullptr,FILE_BEGIN);
            LONGLONG elapsed=0;
            for(DWORD i=0;i<blocks;++i){
                if(stop.stop_requested()){Fail(L"测试已取消，临时文件自动清理");return;}
                if(!phase)memset(data,static_cast<int>((i*17+23)&255),block);else memset(data,0,block);
                LARGE_INTEGER start,end;QueryPerformanceCounter(&start);
                const BOOL ok=phase?ReadFile(file.value,data,block,&bytes,nullptr):WriteFile(file.value,data,block,&bytes,nullptr);
                QueryPerformanceCounter(&end);elapsed+=end.QuadPart-start.QuadPart;
                if(!ok||bytes!=block){Fail(md::ErrorText(ok?ERROR_HANDLE_EOF:GetLastError()));return;}
                if(phase){const auto expected=static_cast<unsigned char>((i*17+23)&255);
                    for(DWORD k=0;k<block;++k)if(data[k]!=expected){Fail(L"数据校验失败");return;}}
                {std::lock_guard lock(mutex_);result_.Progress=(phase*blocks+i+1)/double(2*blocks);}
            }
            speeds[phase]=elapsed?double(block)*blocks/MD_MIB*frequency.QuadPart/elapsed:0;
        }
        std::lock_guard lock(mutex_);result_.Running=false;result_.Read=speeds[1];result_.Write=speeds[0];
        result_.Message="完成 · 全量数据校验通过 · 临时文件自动清理";
    }
public:
    ~Runner(){worker_.request_stop();}
    Result Snapshot(){std::lock_guard lock(mutex_);return result_;}
    void Start(MD_DISK_INFO disk){
        if(Snapshot().Running)return;
        if(worker_.joinable())worker_.join();
        {std::lock_guard lock(mutex_);result_={};result_.Running=true;result_.Id=disk.Id;result_.Message="正在测试…";}
        worker_=std::jthread([this,disk](std::stop_token stop){Run(disk,stop);});
    }
    void Stop(){worker_.request_stop();}
};
}
void Benchmark(const MD_DISK_INFO& disk){
    static Runner runner;const auto result=runner.Snapshot();
    ImGui::Text("当前目标：实例 %llu · %c:",disk.Id,disk.Letter);
    ImGui::TextWrapped("顺序读写 · 16 MiB 文件 · 1 MiB 块 · QD1 · 绕过系统缓存 · 同步写穿透");
    ImGui::TextWrapped("读取后逐字节校验，数据准备和校验耗时不计入 I/O 吞吐。测试文件关闭时自动删除。");
    ImGui::BeginDisabled(result.Running||!disk.Mounted);
    if(ImGui::Button("开始真实测试"))runner.Start(disk);ImGui::EndDisabled();
    if(result.Running){ImGui::SameLine();if(ImGui::Button("停止"))runner.Stop();}
    ImGui::ProgressBar(static_cast<float>(result.Progress),ImVec2(-1,0));
    if(result.Id)ImGui::Text("结果所属实例：%llu",result.Id);
    ImGui::TextWrapped("%s",result.Message.c_str());
    if(result.Read>0)ImGui::Text("读取 %.1f MiB/s · 写入 %.1f MiB/s",result.Read,result.Write);
    ImGui::TextDisabled("当前版本仅提供此固定顺序负载；随机/混合/多并发待后续实现。");
}
}
