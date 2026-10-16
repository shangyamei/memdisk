#include "app.h"
#include <cstdio>
#include <shellapi.h>

namespace ui {
namespace {
std::string Bytes(ULONGLONG value){
    char text[40]{};
    if(value>=1024*MD_MIB)snprintf(text,sizeof(text),"%.2f GiB",value/(1024.*MD_MIB));
    else snprintf(text,sizeof(text),"%.1f MiB",value/double(MD_MIB));
    return text;
}
const Space* CurrentSpace(const State& state,const MD_DISK_INFO& disk){
    if(!disk.Mounted||state.Reply.Error)return nullptr;
    for(const auto& space:state.Spaces)
        if(space.Id==disk.Id&&space.Valid&&space.Total&&space.Free<=space.Total)return &space;
    return nullptr;
}
void DiskTile(Model& model,const State& state,const MD_DISK_INFO& disk,float height,DiskUiActions& actions){
    const bool selected=disk.Id==state.Selected;
    ImGui::PushID(static_cast<int>(disk.Id));
    if(selected)ImGui::PushStyleColor(ImGuiCol_ChildBg,ImGui::GetStyleColorVec4(ImGuiCol_Header));
    ImGui::BeginChild("DiskTile",ImVec2(-1,height),ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    const auto title=std::string(1,static_cast<char>(disk.Letter))+": "+Utf8(disk.Label)+" · "+
        (disk.FileSystem==MdNtfs?"NTFS":"exFAT")+" · "+(disk.Mounted?"已挂载":"已卸载");
    if(ImGui::Selectable(title.c_str(),false)&&!selected)model.Select(disk.Id);
    const auto* space=CurrentSpace(state,disk);
    float fraction=0;
    if(space){
        const auto used=space->Total-space->Free;
        fraction=static_cast<float>(double(used)/space->Total);
        ImGui::Text("已用 %s / 共 %s",Bytes(used).c_str(),Bytes(space->Total).c_str());
    }else{
        ImGui::Text("总容量 %s",Bytes(disk.Bytes).c_str());
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,4*Host.Dpi);
    ImGui::ProgressBar(fraction,ImVec2(-1,8*Host.Dpi),"");
    ImGui::PopStyleVar(2);
    if(ImGui::IsItemHovered()){
        ImGui::BeginTooltip();
        ImGui::Text("磁盘逻辑容量：%s",Bytes(disk.Bytes).c_str());
        if(space){
            ImGui::Text("文件系统总空间：%s",Bytes(space->Total).c_str());
            ImGui::Text("已用 %.1f%% · 可用 %s",fraction*100,Bytes(space->Free).c_str());
            ImGui::TextUnformatted("进度条按文件系统实际空间计算，每秒更新。");
        }else ImGui::TextUnformatted(disk.Mounted?"空间暂不可用或等待后台采样，不代表已用为零。":"已卸载，暂不查询文件空间。");
        ImGui::TextUnformatted("删除文件不释放预分配内存。");
        ImGui::EndTooltip();
    }
    if(space)ImGui::TextDisabled("可用 %s · 已用 %.1f%%",Bytes(space->Free).c_str(),fraction*100);
    else ImGui::TextDisabled("%s",disk.Mounted?"等待空间采样 / 暂不可用":"已卸载 · 空间未采样");
    ImGui::BeginDisabled(state.Busy);
    if(ImGui::Button(disk.Mounted?"卸载":"挂载"))
        model.Submit(md::Request(disk.Mounted?MdUnmount:MdMount,disk.Id));
    ImGui::SameLine();
    if(ImGui::Button("销毁…")){actions.DestroyId=disk.Id;actions.RequestDestroy=true;}
    ImGui::SameLine();ImGui::BeginDisabled(!disk.Mounted);
    if(ImGui::Button("打开目录")){
        const WCHAR path[]={disk.Letter,L':',L'\\',0};
        ShellExecuteW(Host.Handle,L"open",path,nullptr,nullptr,SW_SHOW);
    }
    ImGui::EndDisabled();ImGui::EndDisabled();
    ImGui::EndChild();
    if(selected)ImGui::PopStyleColor();
    ImGui::PopID();
}
}
void DiskList(Model& model,const State& state,DiskUiActions& actions){
    if(!state.Reply.Count){ImGui::TextDisabled("尚无磁盘。请先安装并启动 VM 中的驱动与服务。");return;}
    const float tile=3*ImGui::GetTextLineHeightWithSpacing()+2*ImGui::GetStyle().WindowPadding.y+
        16*Host.Dpi+ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("DiskList",ImVec2(0,0));
    for(ULONG i=0;i<state.Reply.Count;++i)DiskTile(model,state,state.Reply.Disks[i],tile,actions);
    ImGui::EndChild();
}
}
