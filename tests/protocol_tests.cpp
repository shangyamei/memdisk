#include "memdisk/protocol.h"
#include <cstddef>
#include <iostream>
#include <type_traits>
#include "client.h"
#include "memdisk/telemetry.h"
#include <stdexcept>

static_assert(std::is_standard_layout_v<MD_REQUEST>);
static_assert(std::is_trivially_copyable_v<MD_REPLY>);
static_assert(offsetof(MD_REQUEST,Bytes)==16);
static_assert(sizeof(MD_REQUEST)==96);
static_assert(offsetof(MD_DISK_INFO,Counters)%8==0);
static_assert(sizeof(MD_COUNTERS)==(6+MD_LATENCY_BUCKETS)*8);
void TransportTests();
void SecurityTests();
void DetachTests();
static void Require(bool value) { if(!value)throw std::runtime_error("requirement failed"); }
int main(){
    std::cout<<"Shared x64 ABI verified: request="<<sizeof(MD_REQUEST)
        <<", reply="<<sizeof(MD_REPLY)<<"\n";
    try {
        Require(md::ValidLabel(L"Build_01",9));
        Require(!md::ValidLabel(L"bad&cmd",8));
        Require(!md::ValidLabel(L"abcdefghijkl",13));
        Require(!md::ValidLabel(L"not-terminated",3));
        MD_COUNTERS old{},now{};
        now.ReadBytes=4*MD_MIB;now.WriteBytes=MD_MIB;now.Reads=99;now.Writes=1;
        now.Latency[2]=99;now.Latency[10]=1;
        auto d=md::Measure(now,old,.5);
        Require(d.Valid&&d.ReadBytesPerSecond==8*MD_MIB&&d.Iops==200&&d.P99UpperUs==7);
        Require(!md::Measure(now,old,0).Valid);
        Require(!md::Measure(now,old,11).Valid);
        Require(md::Measure(old,old,1).P99UpperUs==-1);
        Require(!md::Measure(old,now,1).Valid);
        now.Latency[2]=0;now.Latency[10]=0;now.Latency[MD_LATENCY_BUCKETS-1]=1;
        Require(md::Measure(now,old,1).P99UpperUs==std::numeric_limits<double>::infinity());
        TransportTests();
        SecurityTests();
        DetachTests();
        std::cout<<"PASS: label policy, rate/percentile/reset handling, named-pipe roundtrips.\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
