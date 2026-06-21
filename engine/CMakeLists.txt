# ----------------------------------------------------------------------------
# Engine library split
# ----------------------------------------------------------------------------
# Three static libs from the same source tree, stamped with different tiers
# via kryga_set_tier() (see cmake/utils.cmake, build_tier.h):
#   engine        — GAME tier. Game with dev tools: console + profiler, no
#                   editor UI, gizmos, or sync service. Used by `kryga_game`
#                   (desktop exe + Android libmain.so for on-device dev runs).
#   engine_editor — DEV tier. Full TU set including editor-only files.
#                   Used by `kryga_editor` exe (desktop only).
#   engine_ship   — SHIP tier. Everything stripped: no ImGui, no console, no
#                   RPC, no editor. Smallest binary. Used by `kryga_ship`
#                   (desktop exe + Android libmain.so, the release artifact).
#
# kryga_engine.cpp / main.cpp are shared across all three via #if KRG_HAS_*
# guards on tier-specific touchpoints (editor singleton, rpc, UI init).

# Tier-agnostic TUs — compile cleanly at every tier (no ImGui, no editor).
set(ENGINE_GAME_SRC
    "public/include/engine/action_queue.h"
    "public/include/engine/config.h"
    "public/include/engine/engine_counters.h"
    "public/include/engine/engine_threads_coordinator.h"
    "public/include/engine/input_manager.h"
    "public/include/engine/kryga_engine.h"
    "public/include/engine/profiler.h"
    "private/src/action_queue.cpp"
    "private/src/config.cpp"
    "private/src/crash_handler.cpp"
    "private/src/engine_counters.cpp"
    "private/src/engine_threads_coordinator.cpp"
    "private/src/input_manager.cpp"
    "private/src/kryga_engine.cpp"
    "private/src/profiler.cpp"
)

# TUs that hard-depend on ImGui (raw <imgui.h>) — only the GAME and DEV tiers.
# Excluded from SHIP, where KRG_HAS_IMGUI=0 and ImGui isn't linked.
set(ENGINE_IMGUI_SRC
    "public/include/engine/console.h"
    "private/src/console.cpp"
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
# engine (game flavor) — GAME tier
# ----------------------------------------------------------------------------
add_library(engine STATIC
    ${ENGINE_GAME_SRC}
    ${ENGINE_IMGUI_SRC}
    ${ENGINE_GENERATED_SRC}
)

kryga_set_tier(engine GAME)

target_link_libraries(engine PUBLIC
    kryga::utils
    kryga::core
    kryga::vulkan_render_impl_game
    kryga::animation
    kryga::asset_importer
    kryga::render_translator
    kryga::audio_translator
    kryga::physics_translator
    kryga::native
    kryga::vfs
    kryga::kryga_port
    kryga::project_paths
    kryga::physics
    kryga::audio
    kryga::physics_translator
    kryga::game_session

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
# engine_editor (editor flavor) — DEV tier
# ----------------------------------------------------------------------------
if(KRG_BUILD_EDITOR_TARGET)
    add_library(engine_editor STATIC
        ${ENGINE_GAME_SRC}
        ${ENGINE_IMGUI_SRC}
        ${ENGINE_EDITOR_SRC}
        ${ENGINE_GENERATED_SRC}
    )

    kryga_set_tier(engine_editor DEV)

    target_link_libraries(engine_editor PUBLIC
        kryga::utils
        kryga::core
        kryga::vulkan_render_impl_editor
        kryga::animation
        kryga::asset_importer
        kryga::render_translator
        kryga::audio_translator
        kryga::physics_translator
        kryga::native
        kryga::vfs
        kryga::kryga_port
        kryga::asset_converter
        kryga::project_paths
        kryga::physics
        kryga::audio
        kryga::physics_translator
        kryga::game_session
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
# engine_ship (shipping flavor) — SHIP tier
# ----------------------------------------------------------------------------
# Tier-agnostic TUs only (no ENGINE_IMGUI_SRC, no ENGINE_EDITOR_SRC). Links the
# no-ImGui render impl. No rpc / asset_converter / imguizmo. TracyClient stays
# linked (profiler calls compile out via KRG_HAS_PROFILER=0; the lib is dead
# weight, harmless — strip later if binary size matters).
add_library(engine_ship STATIC
    ${ENGINE_GAME_SRC}
    ${ENGINE_GENERATED_SRC}
)

kryga_set_tier(engine_ship SHIP)

target_link_libraries(engine_ship PUBLIC
    kryga::utils
    kryga::core
    kryga::vulkan_render_impl_ship
    kryga::animation
    kryga::asset_importer
    kryga::render_translator
    kryga::audio_translator
    kryga::native
    kryga::vfs
    kryga::kryga_port
    kryga::project_paths
    kryga::physics
    kryga::audio
    kryga::physics_translator
    kryga::game_session

    ${KRG_ACTIVE_MODULES_TARGETS}

    sdl2

    TracyClient

    CLI11::CLI11
)

target_include_directories(engine_ship
    PUBLIC
        ${CMAKE_BINARY_DIR}/kryga_generated
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/private/include"
)

kryga_finalize_library(engine_ship)

# ----------------------------------------------------------------------------
# Executables
# ----------------------------------------------------------------------------
# Android packs exactly one libmain.so (SDL's loader entrypoint) per APK, so the
# GAME and SHIP entry libs both use OUTPUT_NAME "main" but live in separate
# output dirs — otherwise ninja sees two rules generating the same file. They are
# alternatives: pick one per build (build_android.sh <target> / gradle build
# type). No reconfigure needed to switch.

# kryga_game — GAME tier (game + dev tools: console, profiler). Desktop exe; on
# Android a SHARED libmain.so for on-device runs with dev tools.
if(ANDROID)
    add_library(kryga_game SHARED "private/src/app/main.cpp")
    set_target_properties(kryga_game PROPERTIES
        OUTPUT_NAME "main"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/android_libs/game")
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

# kryga_ship — SHIP tier (everything stripped). The shipping artifact: a normal
# exe on desktop, and libmain.so on Android (the default release entry).
if(ANDROID)
    add_library(kryga_ship SHARED "private/src/app/main.cpp")
    set_target_properties(kryga_ship PROPERTIES
        OUTPUT_NAME "main"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/android_libs/ship")
    target_link_libraries(kryga_ship PRIVATE log android)
else()
    add_executable(kryga_ship "private/src/app/main.cpp")
    kryga_finalize_executable(kryga_ship)
endif()

target_include_directories(kryga_ship
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(kryga_ship PRIVATE
    engine_ship
)

add_subdirectory(private/tests/engine_regression)
