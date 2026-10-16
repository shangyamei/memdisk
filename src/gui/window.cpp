#include "app.h"
#include <imgui_impl_win32.h>
#include <windowsx.h>
#include <dwmapi.h>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);

namespace ui {
static LRESULT HitTest(HWND hwnd, LPARAM lp) {
    POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; ScreenToClient(hwnd,&p);
    RECT r{}; GetClientRect(hwnd,&r);
    const int border=static_cast<int>(7*Host.Dpi);
    if (!IsZoomed(hwnd)) {
        const bool l=p.x<border, right=p.x>=r.right-border, top=p.y<border, bottom=p.y>=r.bottom-border;
        if(top&&l)return HTTOPLEFT; if(top&&right)return HTTOPRIGHT;
        if(bottom&&l)return HTBOTTOMLEFT; if(bottom&&right)return HTBOTTOMRIGHT;
        if(l)return HTLEFT; if(right)return HTRIGHT; if(top)return HTTOP; if(bottom)return HTBOTTOM;
    }
    if(p.y<48*Host.Dpi && p.x<r.right-220*Host.Dpi)return HTCAPTION;
    return HTCLIENT;
}
static LRESULT CALLBACK Procedure(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_NCCALCSIZE && wp)return 0;
    if(msg==WM_NCHITTEST)return HitTest(hwnd,lp);
    if(msg==WM_GETMINMAXINFO) {
        auto* info=reinterpret_cast<MINMAXINFO*>(lp);
        info->ptMinTrackSize={static_cast<LONG>(400*Host.Dpi),static_cast<LONG>(600*Host.Dpi)};
        MONITORINFO monitor{sizeof(monitor)};
        if(GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor)) {
            info->ptMaxPosition={monitor.rcWork.left-monitor.rcMonitor.left,monitor.rcWork.top-monitor.rcMonitor.top};
            info->ptMaxSize={monitor.rcWork.right-monitor.rcWork.left,monitor.rcWork.bottom-monitor.rcWork.top};
        }
        return 0;
    }
    if(msg==WM_DPICHANGED) {
        const float previous=Host.Dpi; Host.Dpi=HIWORD(wp)/96.f;
        if(ImGui::GetCurrentContext()) {
            ImGui::GetStyle().ScaleAllSizes(Host.Dpi/previous);
            ImGui::GetIO().FontGlobalScale*=Host.Dpi/previous;
        }
        const RECT* rect=reinterpret_cast<RECT*>(lp);
        SetWindowPos(hwnd,nullptr,rect->left,rect->top,rect->right-rect->left,
            rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);
        return 0;
    }
    if(ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd,msg,wp,lp))return 1;
    if(msg==WM_SIZE && Host.Device && wp!=SIZE_MINIMIZED){Host.Resize();return 0;}
    if(msg==WM_DESTROY){Host.Running=false;PostQuitMessage(0);return 0;}
    if(msg==WM_SYSCOMMAND && (wp&0xfff0)==SC_KEYMENU)return 0;
    return DefWindowProcW(hwnd,msg,wp,lp);
}
bool Window::Create(HINSTANCE instance) {
    WNDCLASSEXW wc{sizeof(wc)};wc.style=CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc=Procedure;wc.hInstance=instance;wc.lpszClassName=L"MemDiskWindow";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassExW(&wc);
    Dpi=GetDpiForSystem()/96.f;
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    const float fit=(std::min)(1.f,(work.bottom-work.top-40)/(900.f*Dpi));
    const int width=static_cast<int>(600*Dpi*fit),height=static_cast<int>(900*Dpi*fit);
    Handle=CreateWindowExW(WS_EX_APPWINDOW,wc.lpszClassName,L"MemDisk",
        WS_POPUP|WS_THICKFRAME|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,
        work.left+30,work.top+20,width,height,nullptr,nullptr,instance,nullptr);
    if(!Handle)return false;
    DXGI_SWAP_CHAIN_DESC swap{};swap.BufferCount=2;swap.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    swap.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;swap.OutputWindow=Handle;
    swap.SampleDesc.Count=1;swap.Windowed=TRUE;swap.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    auto result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&swap,&Swap,&Device,nullptr,&Context);
    if(FAILED(result))result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&swap,&Swap,&Device,nullptr,&Context);
    if(FAILED(result))return false;
    Resize();ShowWindow(Handle,SW_SHOWDEFAULT);UpdateWindow(Handle);return true;
}
void Window::Resize() {
    if(Target){Target->Release();Target=nullptr;}
    if(!Swap)return;
    Context->OMSetRenderTargets(0,nullptr,nullptr);
    if(FAILED(Swap->ResizeBuffers(0,0,0,DXGI_FORMAT_UNKNOWN,0)))return;
    ID3D11Texture2D* buffer=nullptr;
    if(SUCCEEDED(Swap->GetBuffer(0,IID_PPV_ARGS(&buffer)))) {
        Device->CreateRenderTargetView(buffer,nullptr,&Target);buffer->Release();
    }
}
void Window::Destroy() {
    if(Target)Target->Release();if(Swap)Swap->Release();if(Context)Context->Release();if(Device)Device->Release();
    if(IsWindow(Handle))DestroyWindow(Handle);
}
}
