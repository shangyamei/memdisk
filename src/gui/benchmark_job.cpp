#include "benchmark.h"

namespace ui {
BenchmarkRunner::~BenchmarkRunner(){Shutdown();}
BenchmarkResult BenchmarkRunner::Snapshot(){std::lock_guard lock(mutex_);return result_;}
bool BenchmarkRunner::Start(const MD_DISK_INFO& disk,BenchmarkWork work){
    if(Snapshot().Running||!disk.Mounted||!work)return false;
    if(worker_.joinable())worker_.join();
    {
        std::lock_guard lock(mutex_);
        result_={};result_.Running=true;result_.Id=disk.Id;result_.Message="正在测试…";
    }
    try{
        worker_=std::jthread([this,disk,work](std::stop_token stop){
            BenchmarkResult completed;
            try{
                completed=work(disk,stop,[this](double value){
                    std::lock_guard lock(mutex_);result_.Progress=value;
                });
            }catch(...){completed.Message="测试异常，资源已清理";}
            // Work owns all I/O resources. Finish cleanup before publishing idle.
            std::lock_guard lock(mutex_);
            completed.Id=disk.Id;completed.Running=false;result_=std::move(completed);
        });
    }catch(...){
        std::lock_guard lock(mutex_);result_.Running=false;result_.Message="无法启动测试线程";
        return false;
    }
    return true;
}
void BenchmarkRunner::Stop(){
    if(!worker_.joinable())return;
    worker_.request_stop();
    std::lock_guard lock(mutex_);
    if(result_.Running)result_.Message="正在停止并关闭测试文件…";
}
void BenchmarkRunner::Shutdown(){Stop();if(worker_.joinable())worker_.join();}
}
