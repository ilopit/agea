file(GLOB ENGINE_SRC           "public/include/engine/*.h"    "private/src/*.cpp")
file(GLOB ENGINE_UI_SRC        "public/include/engine/ui/*.h" "private/src/ui/*.cpp")
file(GLOB ENGINE_GENERATED_SRC "${CMAKE_BINARY_DIR}/kryga_generated/engine/*.cpp")

add_library(engine STATIC
   ${ENGINE_SRC}
   ${ENGINE_UI_SRC}
   ${ENGINE_GENERATED_SRC}
)

target_link_libraries(engine PUBLIC
    kryga::utils
    kryga::core
    kryga::vulkan_render
    kryga::animation
    kryga::asset_importer
    kryga::render_bridge
    kryga::native

    kryga::textedit_unofficial
    kryga::imguizmo_unofficial

    ${KRG_ACTIVE_MODULES_TARGETS}

    boost_beast
    Boost::url

    sdl2

    TracyClient
)

target_include_directories(engine 
    PUBLIC 
        ${CMAKE_BINARY_DIR}/kryga_generated
    PRIVATE 
        "${CMAKE_CURRENT_SOURCE_DIR}/private/include" 
)

target_precompile_headers(engine PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:<imgui.h$<ANGLE-R>>"
)

kryga_finalize_library(engine)

add_executable(engine_app "private/src/app/main.cpp")

target_include_directories(engine_app 
    PUBLIC 
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(engine_app
    engine
)

kryga_finalize_executable(engine_app)