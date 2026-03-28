FetchContent_GetProperties(imguizmo)

add_library(imguizmo_unofficial STATIC)

target_include_directories(imguizmo_unofficial PUBLIC "${imguizmo_SOURCE_DIR}")

target_sources(imguizmo_unofficial PRIVATE
    "${imguizmo_SOURCE_DIR}/ImGuizmo.h"
    "${imguizmo_SOURCE_DIR}/ImGuizmo.cpp"
)

target_link_libraries(imguizmo_unofficial kryga::imgui_unofficial)
add_library(kryga::imguizmo_unofficial ALIAS imguizmo_unofficial)
