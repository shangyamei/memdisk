#include "app.h"

namespace ui {
void Benchmark(Model& model,const MD_DISK_INFO& disk,const State& state){
    const auto& result=state.Benchmark;
    ImGui::Text("当前目标：实例 %llu · %c:",disk.Id,disk.Letter);
    ImGui::TextWrapped("顺序读写 · 16 MiB 文件 · 1 MiB 块 · QD1 · 绕过系统缓存 · 同步写穿透");
    ImGui::TextWrapped("读取后逐字节校验，数据准备和校验耗时不计入 I/O 吞吐。测试文件关闭时自动删除。");
    ImGui::BeginDisabled(result.Running||state.Busy||!disk.Mounted);
    if(ImGui::Button("开始真实测试"))model.StartBenchmark(disk.Id);
    ImGui::EndDisabled();
    if(result.Running){ImGui::SameLine();if(ImGui::Button("停止"))model.StopBenchmark();}
    ImGui::ProgressBar(static_cast<float>(result.Progress),ImVec2(-1,0));
    if(result.Id)ImGui::Text("结果所属实例：%llu",result.Id);
    ImGui::TextWrapped("%s",result.Message.c_str());
    if(result.Read>0)ImGui::Text("读取 %.1f MiB/s · 写入 %.1f MiB/s",result.Read,result.Write);
    ImGui::TextDisabled("测试及清理期间，该实例的挂载、卸载和销毁暂停。");
    ImGui::TextDisabled("当前版本仅提供此固定顺序负载；随机/混合/多并发待后续实现。");
}
}
