#include "benchmark.h"
#include <atomic>
#include <stdexcept>

namespace {
void Require(bool value){if(!value)throw std::runtime_error("benchmark lifecycle regression");}
struct Context {
    md::Handle entered{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
    md::Handle finish{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
    md::Handle cleanup{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
    md::Handle release{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
    std::atomic<bool> closed=false;
    int mode=0;
};
Context* current;
ui::BenchmarkResult Work(const MD_DISK_INFO&,std::stop_token stop,const ui::BenchmarkProgress& progress){
    {
        md::Handle resource(CreateEventW(nullptr,TRUE,FALSE,nullptr));
        Require(static_cast<bool>(resource));
        SetEvent(current->entered.value);
        while(!stop.stop_requested()&&WaitForSingleObject(current->finish.value,5)==WAIT_TIMEOUT){}
        progress(.5);
        SetEvent(current->cleanup.value);
        Require(WaitForSingleObject(current->release.value,5000)==WAIT_OBJECT_0);
    }
    current->closed=true;
    if(current->mode==2)throw std::runtime_error("injected worker failure");
    ui::BenchmarkResult result;result.Message=current->mode==1?"failed":"finished";
    return result;
}
void Lifecycle(int mode){
    Context context;context.mode=mode;current=&context;
    ui::BenchmarkRunner runner;
    // Ensure an assertion failure cannot strand the worker in injected cleanup.
    struct Release {Context& C;~Release(){SetEvent(C.finish.value);SetEvent(C.release.value);}} release{context};
    MD_DISK_INFO disk{};disk.Id=17;disk.Mounted=1;
    Require(runner.Start(disk,Work));
    Require(WaitForSingleObject(context.entered.value,5000)==WAIT_OBJECT_0);
    Require(!runner.Start(disk,Work));
    auto request=md::Request(MdUnmount,disk.Id);
    Require(ui::BenchmarkConflicts(runner.Snapshot(),request));
    request.Operation=MdDestroy;
    Require(ui::BenchmarkConflicts(runner.Snapshot(),request));
    request.Id=18;
    Require(!ui::BenchmarkConflicts(runner.Snapshot(),request));
    if(mode==3)runner.Stop();else SetEvent(context.finish.value);
    Require(WaitForSingleObject(context.cleanup.value,5000)==WAIT_OBJECT_0);
    Require(runner.Snapshot().Running&&!context.closed);
    SetEvent(context.release.value);
    runner.Shutdown();
    Require(context.closed&&!runner.Snapshot().Running);
    Require(!ui::BenchmarkConflicts(runner.Snapshot(),md::Request(MdDestroy,disk.Id)));
}
void CancelPendingIo(){
    const auto name=std::wstring(L"\\\\.\\pipe\\MemDisk.BenchmarkTest.")+std::to_wstring(GetCurrentProcessId());
    md::Handle pipe(CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT,1,4096,4096,0,nullptr));
    Require(static_cast<bool>(pipe));
    md::Handle connected(CreateEventW(nullptr,TRUE,FALSE,nullptr));
    OVERLAPPED connection{};connection.hEvent=connected.value;
    const BOOL ready=ConnectNamedPipe(pipe.value,&connection);
    Require(ready||GetLastError()==ERROR_IO_PENDING);
    md::Handle client(CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr));
    Require(static_cast<bool>(client));
    DWORD bytes=0;Require(GetOverlappedResult(pipe.value,&connection,&bytes,TRUE)!=FALSE);
    md::Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));
    std::atomic<DWORD> status=ERROR_IO_PENDING;
    std::jthread worker([&](std::stop_token stop){
        char data{};
        status=ui::TransferBenchmarkBlock(pipe.value,event.value,&data,1,0,true,stop);
    });
    Sleep(100); // Peer sends no data; the read must wait until cancellation.
    worker.request_stop();worker.join();
    Require(status==ERROR_OPERATION_ABORTED);
    // Completion was drained: the same event and pipe can serve the next I/O.
    const char sent='x';Require(WriteFile(client.value,&sent,1,&bytes,nullptr)!=FALSE);
    char received{};
    Require(ui::TransferBenchmarkBlock(pipe.value,event.value,&received,1,0,true,{})==ERROR_SUCCESS);
    Require(received==sent);
}
}
void BenchmarkTests(){for(int mode=0;mode<4;++mode)Lifecycle(mode);CancelPendingIo();}
