FetchContent_GetProperties(imgui)

add_library(imgui_unofficial STATIC)

target_include_directories(imgui_unofficial PUBLIC "${imgui_SOURCE_DIR}")

target_sources(imgui_unofficial PRIVATE
    "${imgui_SOURCE_DIR}/imgui.h"
    "${imgui_SOURCE_DIR}/imgui.cpp"

    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"

    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp"
)

target_link_libraries(imgui_unofficial sdl2 Vulkan::Vulkan)
add_library(kryga::imgui_unofficial ALIAS imgui_unofficial)