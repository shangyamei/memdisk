#include "app.h"
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

namespace ui { Window Host; }
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    using namespace ui;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!Host.Create(instance)) { MessageBoxW(nullptr,L"D3D11 initialization failed",L"MemDisk",MB_ICONERROR); return 1; }
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImPlot::CreateContext();
    auto& io = ImGui::GetIO(); io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImFontConfig config;
    WCHAR windows[MAX_PATH]{}; GetWindowsDirectoryW(windows, MAX_PATH);
    const auto font = Utf8(std::wstring(windows) + L"\\Fonts\\msyh.ttc");
    if (!io.Fonts->AddFontFromFileTTF(font.c_str(), 16*Host.Dpi, &config,
        io.Fonts->GetGlyphRangesChineseSimplifiedCommon())) io.Fonts->AddFontDefault();
    ApplyTheme(false);
    ImGui_ImplWin32_Init(Host.Handle); ImGui_ImplDX11_Init(Host.Device,Host.Context);
    {
        Model model;
        while (Host.Running) {
            MSG message;
            while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                TranslateMessage(&message); DispatchMessageW(&message);
                if (message.message == WM_QUIT) Host.Running=false;
            }
            if (!Host.Running) break;
            if (IsIconic(Host.Handle)) { Sleep(100); continue; }
            if (!Host.Target) { MessageBoxW(Host.Handle,L"Render target unavailable",L"MemDisk",MB_ICONERROR); break; }
            ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
            Draw(model,model.Snapshot());
            ImGui::Render();
            const float clear[4]={0.94f,0.94f,0.96f,1};
            Host.Context->OMSetRenderTargets(1,&Host.Target,nullptr);
            Host.Context->ClearRenderTargetView(Host.Target,clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            Host.Swap->Present(1,0);
        }
    }
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext(); ImGui::DestroyContext(); Host.Destroy();
    return 0;
}
