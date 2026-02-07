add_library(imguizmo_unofficial STATIC)

set(imguizmo_prefix ${PROJECT_SOURCE_DIR}/thirdparty/upstream)

target_include_directories(imguizmo_unofficial PUBLIC "${imguizmo_prefix}/ImGuizmo")

target_sources(imguizmo_unofficial PRIVATE
    "${imguizmo_prefix}/ImGuizmo/ImGuizmo.h"
    "${imguizmo_prefix}/ImGuizmo/ImGuizmo.cpp"
)

target_link_libraries(imguizmo_unofficial kryga::imgui_unofficial)
add_library(kryga::imguizmo_unofficial ALIAS imguizmo_unofficial)
