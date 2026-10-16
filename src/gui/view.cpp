#include "app.h"
#include "memdisk/capacity.h"

namespace ui {
static bool WindowButton(const char* id,int symbol) {
    const float size=32*Host.Dpi;
    const bool clicked=ImGui::InvisibleButton(id,{size,size});
    const auto p=ImGui::GetItemRectMin(),q=ImGui::GetItemRectMax();
    auto* draw=ImGui::GetWindowDrawList();
    if(ImGui::IsItemHovered())draw->AddRectFilled(p,q,ImGui::GetColorU32(ImGuiCol_ButtonHovered),5*Host.Dpi);
    const float cx=(p.x+q.x)/2,cy=(p.y+q.y)/2,a=5*Host.Dpi;
    const auto color=ImGui::GetColorU32(ImGuiCol_Text);
    if(symbol==0)draw->AddLine({cx-a,cy},{cx+a,cy},color,Host.Dpi);
    else if(symbol==1)draw->AddRect({cx-a,cy-a},{cx+a,cy+a},color,0,0,Host.Dpi);
    else {draw->AddLine({cx-a,cy-a},{cx+a,cy+a},color,Host.Dpi);
        draw->AddLine({cx-a,cy+a},{cx+a,cy-a},color,Host.Dpi);}
    return clicked;
}
static void Title() {
    ImGui::TextUnformatted("M  MemDisk");ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth()-205*Host.Dpi);
    static bool dark=false;
    if(ImGui::Button(dark?"浅色":"深色")){dark=!dark;ApplyTheme(dark);}ImGui::SameLine();
    if(WindowButton("Minimize",0))ShowWindow(Host.Handle,SW_MINIMIZE);ImGui::SameLine();
    if(WindowButton(IsZoomed(Host.Handle)?"Restore":"Maximize",1))
        ShowWindow(Host.Handle,IsZoomed(Host.Handle)?SW_RESTORE:SW_MAXIMIZE);
    ImGui::SameLine();
    if(WindowButton("Close",2))PostMessageW(Host.Handle,WM_CLOSE,0,0);
    ImGui::Separator();
}
static void CreateDiskDialog(Model& model) {
    if(!ImGui::BeginPopupModal("创建内存磁盘",nullptr,ImGuiWindowFlags_AlwaysAutoResize))return;
    static LONGLONG mib=256;static int fs=0;static char label[12]="MemDisk";
    ImGui::SetNextItemWidth(190*Host.Dpi);ImGui::InputScalar("容量 MiB",ImGuiDataType_S64,&mib);
    ImGui::SetNextItemWidth(190*Host.Dpi);ImGui::Combo("文件系统",&fs,"NTFS\0exFAT\0");
    ImGui::SetNextItemWidth(190*Host.Dpi);ImGui::InputText("卷标",label,sizeof(label));
    ImGui::TextDisabled("盘符自动分配 R–Z · 全额非分页预分配");
    ImGui::TextDisabled("容量不超过创建时可用内存的 80%，最多 8 盘");
    MEMORYSTATUSEX memory{sizeof(memory)};
    const bool memoryKnown=GlobalMemoryStatusEx(&memory)!=FALSE;
    const auto budget=MdCapacityBudget(memory.ullAvailPhys);
    if(memoryKnown){
        ImGui::Text("系统可用：%.2f GiB",memory.ullAvailPhys/(1024.*MD_MIB));
        ImGui::Text("当前可创建上限：%llu MiB",budget/MD_MIB);
    }else ImGui::TextDisabled("无法获取系统可用内存");
    const bool valid=memoryKnown&&mib>0&&static_cast<ULONGLONG>(mib)<=budget/MD_MIB&&
        static_cast<ULONGLONG>(mib)<=0x7fffffffffffffffull/MD_MIB;
    ImGui::BeginDisabled(!valid);
    if(ImGui::Button("创建并挂载")){
        auto request=md::Request(MdCreate);request.Bytes=static_cast<ULONGLONG>(mib)*MD_MIB;
        request.FileSystem=fs?MdExfat:MdNtfs;
        MultiByteToWideChar(CP_UTF8,0,label,-1,request.Label,_countof(request.Label));
        if(model.Submit(request))ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();ImGui::SameLine();
    if(ImGui::Button("取消"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
}
static void DestroyDialog(Model& model,const State& state,DiskUiActions& actions){
    if(actions.RequestDestroy){ImGui::OpenPopup("销毁磁盘");actions.RequestDestroy=false;}
    if(!ImGui::BeginPopupModal("销毁磁盘",nullptr,ImGuiWindowFlags_AlwaysAutoResize))return;
    const MD_DISK_INFO* target=nullptr;
    for(ULONG i=0;i<state.Reply.Count;++i)if(state.Reply.Disks[i].Id==actions.DestroyId)target=&state.Reply.Disks[i];
    if(target){
        ImGui::Text("实例 %llu · %c: %s",target->Id,target->Letter,Utf8(target->Label).c_str());
        ImGui::Text("容量：%.0f MiB",target->Bytes/double(MD_MIB));
        ImGui::TextUnformatted("全部文件将丢失，预留内存将被释放。");
    }else ImGui::TextUnformatted("该磁盘已不存在，或当前无法确认目标实例。");
    const bool testing=state.Benchmark.Running&&state.Benchmark.Id==actions.DestroyId;
    if(testing)ImGui::TextUnformatted("该实例正在测试或清理，请等待测试结束。");
    ImGui::BeginDisabled(!target||state.Busy||testing);
    if(ImGui::Button("确认销毁")&&target){
        if(model.Submit(md::Request(MdDestroy,actions.DestroyId)))ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();ImGui::SameLine();
    if(ImGui::Button("取消"))ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
void Draw(Model& model,const State& state) {
    const auto* viewport=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);ImGui::SetNextWindowSize(viewport->Size);
    ImGui::Begin("MemDiskRoot",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoScrollWithMouse);
    Title();
    static DiskUiActions actions;
    static bool firstPage=true;
    bool createRequested=false;
    const float footer=ImGui::GetTextLineHeightWithSpacing();
    ImGui::BeginChild("PageBody",ImVec2(0,-footer),ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    if(ImGui::BeginTabBar("MainPages")){
        if(ImGui::BeginTabItem("磁盘管理",nullptr,firstPage?ImGuiTabItemFlags_SetSelected:0)){
            ImGui::BeginDisabled(state.Busy);
            createRequested=ImGui::Button("＋ 创建磁盘");
            ImGui::EndDisabled();ImGui::SameLine();
            ImGui::TextDisabled("%u / %u 个磁盘",state.Reply.Count,MD_MAX_DISKS);
            DiskList(model,state,actions);
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("监控诊断")){
            MonitorPage(model,state);
            ImGui::EndTabItem();
        }
        firstPage=false;
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
    // Popups live at root scope; card actions capture a fixed instance ID.
    if(createRequested)ImGui::OpenPopup("创建内存磁盘");
    CreateDiskDialog(model);DestroyDialog(model,state,actions);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::BeginChild("StatusBar",ImVec2(0,ImGui::GetTextLineHeight()),ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    const auto message=Utf8(state.Reply.Error?state.Reply.Message:state.Message);
    std::string line=state.Reply.Error?"服务未连接":"驱动已连接";
    line+=state.Busy?" · 操作中…":" · 250 ms 采样";
    if(!message.empty())line+=" · "+message;
    for(char& c:line)if(c=='\r'||c=='\n'||c=='\t')c=' ';
    ImGui::TextDisabled("%s",line.c_str());
    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",message.empty()?line.c_str():message.c_str());
    ImGui::EndChild();ImGui::PopStyleVar();ImGui::End();
}
}
