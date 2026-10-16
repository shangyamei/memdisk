#include "client.h"
#include <future>
#include <stdexcept>
#include <thread>
#include <chrono>

static void Roundtrip(ULONG version,bool truncate,DWORD expectedError) {
    std::promise<bool> ready;
    auto future=ready.get_future();
    std::jthread server([&]{
        md::Handle pipe(CreateNamedPipeW(MD_PIPE_PATH,PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT,1,sizeof(MD_REPLY),sizeof(MD_REQUEST),0,nullptr));
        ready.set_value(static_cast<bool>(pipe));
        if(!pipe)return;
        if(!ConnectNamedPipe(pipe.value,nullptr)&&GetLastError()!=ERROR_PIPE_CONNECTED)return;
        MD_REQUEST request{};DWORD bytes=0;
        if(!ReadFile(pipe.value,&request,sizeof(request),&bytes,nullptr))return;
        MD_REPLY reply{};reply.Version=version;reply.Count=1;
        reply.Disks[0].Id=0x123456789ull;reply.Disks[0].Bytes=64*MD_MIB;
        const DWORD size=truncate?sizeof(ULONG):sizeof(reply);
        if(!WriteFile(pipe.value,&reply,size,&bytes,nullptr))return;
        // Tests own both sides; the client immediately consumes the response.
        FlushFileBuffers(pipe.value);
        DisconnectNamedPipe(pipe.value);
    });
    if(!future.get())throw std::runtime_error("Test pipe unavailable (is a real service already running?)");
    const auto reply=md::ServiceCall(md::Request(MdList));
    if(reply.Error!=expectedError)throw std::runtime_error("Unexpected transport error");
    if(!reply.Error&&reply.Disks[0].Id!=0x123456789ull)throw std::runtime_error("64-bit wire data corrupted");
}
static void DelayedPipeTest(){
    std::jthread server([]{
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        md::Handle pipe(CreateNamedPipeW(MD_PIPE_PATH,PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT,1,sizeof(MD_REPLY),sizeof(MD_REQUEST),0,nullptr));
        if(!pipe)return;
        if(!ConnectNamedPipe(pipe.value,nullptr)&&GetLastError()!=ERROR_PIPE_CONNECTED)return;
        MD_REQUEST request{};DWORD bytes=0;
        if(!ReadFile(pipe.value,&request,sizeof(request),&bytes,nullptr))return;
        MD_REPLY reply{};reply.Version=MD_VERSION;
        WriteFile(pipe.value,&reply,sizeof(reply),&bytes,nullptr);FlushFileBuffers(pipe.value);
        DisconnectNamedPipe(pipe.value);
    });
    if(md::ServiceCall(md::Request(MdList)).Error)throw std::runtime_error("Delayed pipe reconnect failed");
}
void TransportTests() {
    Roundtrip(MD_VERSION,false,0);
    Roundtrip(MD_VERSION+1,false,ERROR_INVALID_DATA);
    Roundtrip(MD_VERSION,true,ERROR_INVALID_DATA);
    DelayedPipeTest();
}
