#include "app.h"
#include <cmath>

namespace ui {
static void Plot(const char* title,const char* unit,const std::vector<Sample>& samples,
    int field,float height,double range){
    if(ImPlot::BeginPlot(title,ImVec2(-1,height),ImPlotFlags_NoTitle)){
        ImPlot::SetupAxes("时间 (s)",unit,ImPlotAxisFlags_NoLabel,ImPlotAxisFlags_AutoFit);
        if(!samples.empty())ImPlot::SetupAxisLimits(ImAxis_X1,samples.back().Time-range,samples.back().Time,ImGuiCond_Always);
        std::vector<double> x,a,b;
        for(const auto& s:samples){x.push_back(s.Time);a.push_back(field==0?s.Read:field==1?s.Iops:s.P99>=0?s.P99:NAN);b.push_back(s.Write);}
        if(!x.empty()){
            ImPlot::SetNextLineStyle(ImVec4(.15f,.65f,.68f,1),1.7f);
            ImPlot::PlotLine(field==0?"读取":field==1?"IOPS":"P99 上界",x.data(),a.data(),static_cast<int>(x.size()));
            if(field==0){ImPlot::SetNextLineStyle(ImVec4(.9f,.58f,.28f,1),1.7f);ImPlot::PlotLine("写入",x.data(),b.data(),static_cast<int>(x.size()));}
        }
        ImPlot::EndPlot();
    }
}
void Performance(const State& state){
    static bool frozen=false;static std::vector<Sample> held;static int range=0;
    static ULONGLONG heldId=0;
    if(heldId!=state.Selected){heldId=state.Selected;frozen=false;held.clear();}
    if(ImGui::Checkbox("冻结查看",&frozen)&&frozen)held=state.Samples;
    ImGui::SameLine();ImGui::SetNextItemWidth(105*Host.Dpi);ImGui::Combo("##range",&range,"60 秒\0 5 分钟\0 30 分钟\0");
    ImGui::TextDisabled("设备层真实数据 · P99 为 log2 分桶上界 · 非应用延迟");
    const auto& samples=frozen?held:state.Samples;
    const double seconds=range==0?60.:range==1?300.:1800.;
    const float available=ImGui::GetContentRegionAvail().y;
    const float height=(std::max)(100*Host.Dpi,(available-42*Host.Dpi)/3);
    ImGui::TextUnformatted("设备吞吐量");Plot("Throughput","MiB/s",samples,0,height,seconds);
    ImGui::TextUnformatted("设备 IOPS");Plot("Iops","次/s",samples,1,height,seconds);
    ImGui::TextUnformatted("请求延迟");Plot("Latency","μs",samples,2,height,seconds);
}
void Capacity(const MD_DISK_INFO& disk,const State& state){
    ImGui::Text("预分配数据区：%.2f GiB",disk.Bytes/(1024.*MD_MIB));
    ImGui::Text("驱动元数据下限：%llu B",disk.MetadataBytes);
    ImGui::TextDisabled("元数据值不含系统设备对象和池分配管理开销。");
    ImGui::TextUnformatted("分配策略：全额预分配 · 非分页");
    ImGui::TextWrapped("删除文件或卸载不会释放预留数据区。销毁磁盘才会释放。");
    ImGui::Separator();
    const Space* space=nullptr;
    for(const auto& item:state.Spaces)if(item.Id==disk.Id)space=&item;
    if(space&&space->Valid&&space->Total){
        ImGui::Text("文件已用：%.2f MiB · 可用：%.2f MiB",(space->Total-space->Free)/double(MD_MIB),space->Free/double(MD_MIB));
        ImGui::ProgressBar(float(space->Total-space->Free)/space->Total,ImVec2(-1,7*Host.Dpi),"");
    }else ImGui::TextDisabled("文件空间不可用（未挂载或等待采样）");
    ImGui::Text("累计读取：%.2f MiB",disk.Counters.ReadBytes/double(MD_MIB));
    ImGui::Text("累计写入：%.2f MiB",disk.Counters.WriteBytes/double(MD_MIB));
}
}
