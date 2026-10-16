#pragma once
#include "client.h"
#include <d3d11.h>
#include <imgui.h>
#include <implot.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace ui {
struct Sample { double Time=0, Read=0, Write=0, Iops=0, P99=0; };
struct Space { ULONGLONG Id=0,Total=0,Free=0; bool Valid=false; };
struct DiskUiActions { ULONGLONG DestroyId=0; bool RequestDestroy=false; };
struct State {
    MD_REPLY Reply{};
    ULONGLONG Selected=0;
    std::vector<Sample> Samples;
    std::vector<Space> Spaces;
    std::wstring Message;
    bool Busy=false;
};
class Model {
    std::mutex mutex_;
    State state_;
    std::optional<MD_REQUEST> pending_;
    std::jthread worker_;
    void Run(std::stop_token stop);
public:
    Model();
    ~Model();
    State Snapshot();
    void Select(ULONGLONG id);
    bool Submit(const MD_REQUEST& request);
};
struct Window {
    HWND Handle=nullptr;
    ID3D11Device* Device=nullptr;
    ID3D11DeviceContext* Context=nullptr;
    IDXGISwapChain* Swap=nullptr;
    ID3D11RenderTargetView* Target=nullptr;
    bool Running=true;
    float Dpi=1;
    bool Create(HINSTANCE instance);
    void Resize();
    void Destroy();
};
extern Window Host;
void ApplyTheme(bool dark);
void Draw(Model& model, const State& state);
void DiskList(Model& model,const State& state,DiskUiActions& actions);
void MonitorPage(Model& model,const State& state);
void Performance(const State& state);
void Capacity(const MD_DISK_INFO& disk,const State& state);
void Benchmark(const MD_DISK_INFO& disk);
std::string Utf8(const std::wstring& value);
}
