#include "app.h"

namespace ui {
void MonitorPage(Model& model,const State& state){
    const MD_DISK_INFO* selected=nullptr;
    for(ULONG i=0;i<state.Reply.Count;++i)if(state.Reply.Disks[i].Id==state.Selected)selected=&state.Reply.Disks[i];
    std::string preview=selected?std::string(1,static_cast<char>(selected->Letter))+": "+Utf8(selected->Label):"选择监控磁盘";
    ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##MonitorDisk",preview.c_str())){
        for(ULONG i=0;i<state.Reply.Count;++i){const auto& disk=state.Reply.Disks[i];
            ImGui::PushID(static_cast<int>(disk.Id));
            const auto label=std::string(1,static_cast<char>(disk.Letter))+": "+Utf8(disk.Label);
            if(ImGui::Selectable(label.c_str(),disk.Id==state.Selected))model.Select(disk.Id);
            if(disk.Id==state.Selected)ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if(selected&&!state.Samples.empty()){
        const auto& s=state.Samples.back();
        ImGui::Text("读 %.1f MiB/s   写 %.1f MiB/s",s.Read,s.Write);
        if(s.P99>=0)ImGui::Text("IOPS %.0f   P99 ≤ %.0f μs   在途 %llu",s.Iops,s.P99,selected->Counters.InFlight);
        else ImGui::Text("IOPS %.0f   P99 —   在途 %llu",s.Iops,selected->Counters.InFlight);
    }else ImGui::TextDisabled(selected?"等待设备性能采样…":"暂无磁盘，可先到磁盘管理创建。");
    ImGui::BeginChild("MonitorWorkspace",ImVec2(0,0),ImGuiChildFlags_Borders);
    if(ImGui::BeginTabBar("MonitorTabs")){
        if(ImGui::BeginTabItem("实时性能")){
            if(selected)Performance(state);else ImGui::TextDisabled("尚未选择磁盘。");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("容量内存")){
            if(selected)Capacity(*selected,state);else ImGui::TextDisabled("尚未选择磁盘。");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("基准测试")){
            if(selected)Benchmark(*selected);else ImGui::TextDisabled("尚未选择磁盘。");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("事件诊断")){
            ImGui::Text("连接错误码：%lu",state.Reply.Error);
            if(state.Reply.Error)ImGui::TextWrapped("%s",Utf8(state.Reply.Message).c_str());
            else ImGui::TextUnformatted("当前连接正常");
            ImGui::TextWrapped("最近操作：%s",Utf8(state.Message).c_str());
            if(selected)ImGui::Text("数据读写失败累计：%llu",selected->Counters.Errors);
            ImGui::TextDisabled("此版本显示最近操作结果，完整事件历史尚未实现。");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}
}
