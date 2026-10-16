#include "app.h"

namespace ui {
std::string Utf8(const std::wstring& value) {
    const auto size=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    std::string result(size,'\0');
    WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),size,nullptr,nullptr);
    return result;
}
void ApplyTheme(bool dark) {
    if(dark)ImGui::StyleColorsDark();else ImGui::StyleColorsLight();
    auto& s=ImGui::GetStyle();
    s.WindowRounding=0;s.ChildRounding=10;s.FrameRounding=6;s.PopupRounding=10;
    s.GrabRounding=5;s.TabRounding=6;s.WindowPadding={16,12};s.ItemSpacing={9,8};
    s.FramePadding={10,6};s.WindowBorderSize=0;s.ChildBorderSize=1;
    s.FrameBorderSize=1;s.PopupBorderSize=1;
    s.Colors[ImGuiCol_WindowBg]=dark?ImVec4(.125f,.13f,.145f,1):ImVec4(.945f,.945f,.96f,1);
    s.Colors[ImGuiCol_ChildBg]=dark?ImVec4(.17f,.175f,.19f,1):ImVec4(1,1,1,1);
    s.Colors[ImGuiCol_Border]=dark?ImVec4(.25f,.26f,.29f,1):ImVec4(.85f,.86f,.89f,1);
    s.Colors[ImGuiCol_Header]=dark?ImVec4(.17f,.25f,.36f,1):ImVec4(.86f,.91f,.99f,1);
    s.Colors[ImGuiCol_Button]=dark?ImVec4(.24f,.25f,.28f,1):ImVec4(.90f,.91f,.94f,1);
    // Inputs and combo previews must remain distinct from an opaque popup.
    s.Colors[ImGuiCol_PopupBg]=dark?ImVec4(.16f,.17f,.19f,1):ImVec4(.99f,.99f,.995f,1);
    s.Colors[ImGuiCol_FrameBg]=dark?ImVec4(.22f,.235f,.27f,1):ImVec4(.925f,.932f,.95f,1);
    s.Colors[ImGuiCol_FrameBgHovered]=dark?ImVec4(.28f,.31f,.36f,1):ImVec4(.87f,.89f,.93f,1);
    s.Colors[ImGuiCol_FrameBgActive]=dark?ImVec4(.31f,.35f,.42f,1):ImVec4(.82f,.86f,.92f,1);
    s.Colors[ImGuiCol_Border]=dark?ImVec4(.34f,.36f,.40f,1):ImVec4(.70f,.73f,.79f,1);
    s.Colors[ImGuiCol_TextDisabled]=dark?ImVec4(.65f,.68f,.73f,1):ImVec4(.40f,.43f,.48f,1);
    s.Colors[ImGuiCol_PlotHistogram]=dark?ImVec4(.38f,.62f,.95f,1):ImVec4(.24f,.48f,.78f,1);
    ImPlot::StyleColorsAuto();s.ScaleAllSizes(Host.Dpi);
}
}
