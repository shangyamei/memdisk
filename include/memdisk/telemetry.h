#pragma once
#include "protocol.h"
#include <limits>

namespace md {
struct Delta {
    bool Valid=false;
    double ReadBytesPerSecond=0, WriteBytesPerSecond=0, Iops=0;
    double P99UpperUs=-1;
};
inline Delta Measure(const MD_COUNTERS& now,const MD_COUNTERS& old,double seconds) {
    Delta result;
    if (!(seconds>0) || seconds>10 || now.ReadBytes<old.ReadBytes ||
        now.WriteBytes<old.WriteBytes || now.Reads<old.Reads || now.Writes<old.Writes) return result;
    ULONGLONG count=0;
    for(ULONG i=0;i<MD_LATENCY_BUCKETS;++i) {
        if(now.Latency[i]<old.Latency[i])return result;
        const auto delta=now.Latency[i]-old.Latency[i];
        if(delta>(std::numeric_limits<ULONGLONG>::max)()-count)return result;
        count+=delta;
    }
    result.Valid=true;
    result.ReadBytesPerSecond=(now.ReadBytes-old.ReadBytes)/seconds;
    result.WriteBytesPerSecond=(now.WriteBytes-old.WriteBytes)/seconds;
    result.Iops=((now.Reads-old.Reads)+double(now.Writes-old.Writes))/seconds;
    if(count) {
        const ULONGLONG target=count-count/100; // ceil(99*n/100), without overflow.
        ULONGLONG sum=0;
        for(ULONG i=0;i<MD_LATENCY_BUCKETS;++i) {
            sum+=now.Latency[i]-old.Latency[i];
            if(sum>=target){
                // Last bucket is open-ended, so it has no finite upper bound.
                result.P99UpperUs=i==MD_LATENCY_BUCKETS-1?
                    std::numeric_limits<double>::infinity():double((1ull<<(i+1))-1);
                break;
            }
        }
    }
    return result;
}
}
