add_library(imgui_unofficial STATIC)

set(imgui_prefix ${PROJECT_SOURCE_DIR}/thirdparty/upstream)

target_include_directories(imgui_unofficial PUBLIC "${imgui_prefix}/imgui")

target_sources(imgui_unofficial PRIVATE 
    "${imgui_prefix}/imgui/imgui.h"
    "${imgui_prefix}/imgui/imgui.cpp"
    
    "${imgui_prefix}/imgui/imgui_demo.cpp"
    "${imgui_prefix}/imgui/imgui_draw.cpp"
    "${imgui_prefix}/imgui/imgui_widgets.cpp"
    "${imgui_prefix}/imgui/imgui_tables.cpp"

    "${imgui_prefix}/imgui/backends/imgui_impl_vulkan.cpp"
    "${imgui_prefix}/imgui/backends/imgui_impl_sdl.cpp"
)

target_link_libraries(imgui_unofficial sdl2 Vulkan::Vulkan)