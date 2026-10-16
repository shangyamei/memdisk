#pragma once
#include "client.h"
#include <functional>
#include <mutex>
#include <thread>

namespace ui {
struct BenchmarkResult {
    bool Running=false;
    double Progress=0,Read=0,Write=0;
    std::string Message="等待测试";
    ULONGLONG Id=0;
};
using BenchmarkProgress=std::function<void(double)>;
using BenchmarkWork=BenchmarkResult(*)(const MD_DISK_INFO&,std::stop_token,const BenchmarkProgress&);
BenchmarkResult RunBenchmark(const MD_DISK_INFO&,std::stop_token,const BenchmarkProgress&);
DWORD TransferBenchmarkBlock(HANDLE,HANDLE,void*,DWORD,ULONGLONG,bool,std::stop_token);
inline bool BenchmarkConflicts(const BenchmarkResult& result,const MD_REQUEST& request){
    return result.Running&&result.Id==request.Id&&
        (request.Operation==MdMount||request.Operation==MdUnmount||request.Operation==MdDestroy);
}
class BenchmarkRunner {
    std::mutex mutex_;
    BenchmarkResult result_;
    std::jthread worker_;
public:
    ~BenchmarkRunner();
    BenchmarkResult Snapshot();
    bool Start(const MD_DISK_INFO&,BenchmarkWork work=RunBenchmark);
    void Stop();
    void Shutdown();
};
}
