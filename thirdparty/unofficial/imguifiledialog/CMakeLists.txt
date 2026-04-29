FetchContent_GetProperties(imguifiledialog)

add_library(imguifiledialog_unofficial STATIC)

target_include_directories(imguifiledialog_unofficial PUBLIC "${imguifiledialog_SOURCE_DIR}")

target_sources(imguifiledialog_unofficial PRIVATE
    "${imguifiledialog_SOURCE_DIR}/ImGuiFileDialog.h"
    "${imguifiledialog_SOURCE_DIR}/ImGuiFileDialog.cpp"
)

target_link_libraries(imguifiledialog_unofficial kryga::imgui_unofficial)
add_library(kryga::imguifiledialog_unofficial ALIAS imguifiledialog_unofficial)
