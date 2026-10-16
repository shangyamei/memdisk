#include "app.h"
#include "memdisk/telemetry.h"
#include <chrono>

namespace ui {
Model::Model():worker_([this](std::stop_token stop){Run(stop);}){}
Model::~Model(){worker_.request_stop();CancelSynchronousIo(worker_.native_handle());}
State Model::Snapshot(){std::lock_guard lock(mutex_);return state_;}
void Model::Select(ULONGLONG id){
    std::lock_guard lock(mutex_);if(state_.Selected==id)return;
    state_.Selected=id;state_.Samples.clear();
}
bool Model::Submit(const MD_REQUEST& request){
    std::lock_guard lock(mutex_);if(state_.Busy)return false;
    pending_=request;state_.Busy=true;state_.Message=L"操作进行中…";return true;
}
void Model::Run(std::stop_token stop){
    MD_DISK_INFO previous{};ULONGLONG previousTick=0,spaceTick=0;
    while(!stop.stop_requested()){
        std::optional<MD_REQUEST> request;
        {std::lock_guard lock(mutex_);request=pending_;pending_.reset();}
        if(request){
            const auto reply=md::ServiceCall(*request);
            std::lock_guard lock(mutex_);state_.Busy=false;
            state_.Message=reply.Error?reply.Message:L"操作完成";
        }
        const auto reply=md::ServiceCall(md::Request(MdList));const ULONGLONG tick=GetTickCount64();
        std::vector<Space> spaces;
        const bool sampleSpace=tick-spaceTick>=1000;
        if(sampleSpace){
            for(ULONG i=0;i<reply.Count;++i){const auto& d=reply.Disks[i];Space space;space.Id=d.Id;
                const WCHAR path[]={d.Letter,L':',L'\\',0};ULARGE_INTEGER total{},free{};
                space.Valid=d.Mounted&&GetDiskFreeSpaceExW(path,nullptr,&total,&free);
                space.Total=total.QuadPart;space.Free=free.QuadPart;spaces.push_back(space);}
            spaceTick=tick;
        }
        {
            std::lock_guard lock(mutex_);state_.Reply=reply;if(sampleSpace)state_.Spaces=std::move(spaces);
            if(reply.Error) previousTick=0;
            const MD_DISK_INFO* selected=nullptr;
            for(ULONG i=0;i<reply.Count;++i)if(reply.Disks[i].Id==state_.Selected)selected=&reply.Disks[i];
            if(!selected&&reply.Count){selected=&reply.Disks[0];state_.Selected=selected->Id;state_.Samples.clear();}
            if(selected){
                if(previous.Id==selected->Id&&previousTick&&tick>previousTick){
                    const double dt=(tick-previousTick)/1000.;
                    const auto delta=md::Measure(selected->Counters,previous.Counters,dt);
                    if(delta.Valid){
                        Sample sample;sample.Time=tick/1000.;
                        sample.Read=delta.ReadBytesPerSecond/MD_MIB;
                        sample.Write=delta.WriteBytesPerSecond/MD_MIB;
                        sample.Iops=delta.Iops;
                        sample.P99=delta.P99UpperUs;
                        state_.Samples.push_back(sample);
                        if(state_.Samples.size()>7200)state_.Samples.erase(state_.Samples.begin(),state_.Samples.begin()+100);
                    }
                }
                previous=*selected;previousTick=tick;
            }else{previous={};previousTick=0;state_.Selected=0;state_.Samples.clear();}
        }
        for(int i=0;i<5&&!stop.stop_requested();++i)std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
}
