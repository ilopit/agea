# ----------------------------------------------------------------------------
# Engine library split
# ----------------------------------------------------------------------------
# Two static libs from the same source tree, compiled with different defines:
#   engine        — KRG_GAME=1.   Game-safe TUs only. No editor UI, no
#                   gizmos, no console, no profiler overlay, no script editor,
#                   no sync service. Used by `kryga_game` exe (and is the only
#                   variant on Android).
#   engine_editor — KRG_EDITOR=1. Full TU set including editor-only files.
#                   Used by `kryga_editor` exe (desktop only).
#
# kryga_engine.cpp / main.cpp are shared between the two via #if KRG_EDITOR
# guards on editor-specific touchpoints (game_editor singleton, rpc, UI init).

# All TUs that do not depend on editor-only headers/symbols.
set(ENGINE_GAME_SRC
    "public/include/engine/action_queue.h"
    "public/include/engine/config.h"
    "public/include/engine/console.h"
    "public/include/engine/engine_counters.h"
    "public/include/engine/input_manager.h"
    "public/include/engine/kryga_engine.h"
    "public/include/engine/profiler.h"
    "private/src/action_queue.cpp"
    "private/src/config.cpp"
    "private/src/console.cpp"
    "private/src/engine_counters.cpp"
    "private/src/input_manager.cpp"
    "private/src/kryga_engine.cpp"
    "private/src/profiler.cpp"
)

# TUs that pull in editor singletons / Boost.Beast — excluded from the game lib.
set(ENGINE_EDITOR_SRC
    "public/include/engine/editor.h"
    "public/include/engine/editor_system.h"
    "public/include/engine/ui.h"
    "private/include/engine/private/engine_rpc.h"
    "private/include/engine/private/property_rpc.h"
    "private/src/editor.cpp"
    "private/src/editor_system.cpp"
    "private/src/engine_rpc.cpp"
    "private/src/property_rpc.cpp"
    "private/src/ui.cpp"
    "private/src/ui/bake_editor.cpp"
    "private/src/ui/converter_window.cpp"
    "private/src/ui/gizmo_editor.cpp"
    "private/src/ui/material_previewer.cpp"
    "private/include/engine/private/screenshot_capture.h"
    "private/src/screenshot_capture.cpp"
)

file(GLOB ENGINE_GENERATED_SRC "${CMAKE_BINARY_DIR}/kryga_generated/engine/*.cpp")

# ----------------------------------------------------------------------------
# engine (game flavor) — KRG_BUILD_TEST
# ----------------------------------------------------------------------------
add_library(engine STATIC
    ${ENGINE_GAME_SRC}
    ${ENGINE_GENERATED_SRC}
)

target_compile_definitions(engine PUBLIC
    KRG_BUILD_TEST=1
)

target_link_libraries(engine PUBLIC
    kryga::utils
    kryga::core
    kryga::vulkan_render_impl_game
    kryga::animation
    kryga::asset_importer
    kryga::render_bridge
    kryga::native
    kryga::vfs
    kryga::kryga_port
    kryga::project_paths
    kryga::physics

    ${KRG_ACTIVE_MODULES_TARGETS}

    sdl2

    TracyClient

    CLI11::CLI11
)

target_include_directories(engine
    PUBLIC
        ${CMAKE_BINARY_DIR}/kryga_generated
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private/include"
)

kryga_finalize_library(engine)

# ----------------------------------------------------------------------------
# engine_editor (editor flavor) — KRG_BUILD_DEV
# ----------------------------------------------------------------------------
if(KRG_BUILD_EDITOR_TARGET)
    add_library(engine_editor STATIC
        ${ENGINE_GAME_SRC}
        ${ENGINE_EDITOR_SRC}
        ${ENGINE_GENERATED_SRC}
    )

    target_compile_definitions(engine_editor PUBLIC
        KRG_BUILD_DEV=1
    )

    target_link_libraries(engine_editor PUBLIC
        kryga::utils
        kryga::core
        kryga::vulkan_render_impl_editor
        kryga::animation
        kryga::asset_importer
        kryga::render_bridge
        kryga::native
        kryga::vfs
        kryga::kryga_port
        kryga::asset_converter
        kryga::project_paths
        kryga::physics
        kryga::rpc

        kryga::imguizmo_unofficial

        ${KRG_ACTIVE_MODULES_TARGETS}
        ${KRG_EDITOR_MODULES_TARGETS}

        sdl2

        TracyClient

        CLI11::CLI11

        # Used by the asset converter UI (engine/private/src/ui/converter_window.cpp)
        # to launch external tools.
        Boost::process
    )

    target_include_directories(engine_editor
        PUBLIC
            ${CMAKE_BINARY_DIR}/kryga_generated
            "${CMAKE_CURRENT_SOURCE_DIR}/public/include"
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/private/include"
    )

    target_precompile_headers(engine_editor PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:<imgui.h$<ANGLE-R>>"
    )

    kryga_ide_path(engine_editor)
    add_library(kryga::engine_editor ALIAS engine_editor)
endif()

# ----------------------------------------------------------------------------
# Executables
# ----------------------------------------------------------------------------
# kryga_game — always built. On Android it becomes libmain.so (SDL's loader
# entrypoint); on desktop it is a normal exe.
if(ANDROID)
    add_library(kryga_game SHARED "private/src/app/main.cpp")
    set_target_properties(kryga_game PROPERTIES OUTPUT_NAME "main")
    target_link_libraries(kryga_game PRIVATE log android)
else()
    add_executable(kryga_game "private/src/app/main.cpp")
    kryga_finalize_executable(kryga_game)
endif()

target_include_directories(kryga_game
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(kryga_game PRIVATE
    engine
)

# kryga_editor — desktop only.
if(KRG_BUILD_EDITOR_TARGET)
    add_executable(kryga_editor "private/src/app/main.cpp")
    kryga_finalize_executable(kryga_editor)

    target_include_directories(kryga_editor
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(kryga_editor PRIVATE
        engine_editor
    )
endif()

add_subdirectory(private/tests/engine_regression)
