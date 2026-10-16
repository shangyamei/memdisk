include(FetchContent)
# Build-only sources, pinned upstream. No third-party code is shipped in our source tree.
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG f5befd2d29e66809cd1110a152e375a7f1981f06)
FetchContent_Declare(implot GIT_REPOSITORY https://github.com/epezent/implot.git
  GIT_TAG 18c72431f8265e2b0b5378a3a73d8a883b2175ff)
FetchContent_MakeAvailable(imgui implot)
add_library(memdisk-widgets STATIC ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
  ${implot_SOURCE_DIR}/implot.cpp ${implot_SOURCE_DIR}/implot_items.cpp)
target_include_directories(memdisk-widgets PUBLIC ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends ${implot_SOURCE_DIR})
target_link_libraries(memdisk-widgets PUBLIC d3d11 dxgi dwmapi)
add_executable(memdisk-ui WIN32 src/gui/main.cpp src/gui/window.cpp
  src/gui/model.cpp src/gui/theme.cpp src/gui/view.cpp src/gui/panels.cpp src/gui/benchmark.cpp
  src/gui/disk_list.cpp src/gui/monitor_view.cpp src/gui/benchmark_job.cpp src/gui/benchmark_io.cpp)
target_link_libraries(memdisk-ui PRIVATE memdisk-common memdisk-widgets shell32)
target_link_options(memdisk-ui PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
