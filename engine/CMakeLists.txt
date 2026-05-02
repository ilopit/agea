file(GLOB ENGINE_SRC           "public/include/engine/*.h"    "private/src/*.cpp")
file(GLOB ENGINE_UI_SRC        "public/include/engine/ui/*.h" "private/src/ui/*.cpp")
file(GLOB ENGINE_GENERATED_SRC "${CMAKE_BINARY_DIR}/kryga_generated/engine/*.cpp")

# sync_service.cpp pulls in Boost.Beast/Asio and is only useful for the
# desktop editor's live-reload HTTP server. Exclude it when disabled.
if(NOT KRG_ENABLE_SYNC_SERVICE)
    list(FILTER ENGINE_SRC EXCLUDE REGEX "sync_service\\.cpp$")
endif()

add_library(engine STATIC
   ${ENGINE_SRC}
   ${ENGINE_UI_SRC}
   ${ENGINE_GENERATED_SRC}
)

if(KRG_ENABLE_SYNC_SERVICE)
    target_compile_definitions(engine PUBLIC KRG_ENABLE_SYNC_SERVICE=1)
else()
    target_compile_definitions(engine PUBLIC KRG_ENABLE_SYNC_SERVICE=0)
endif()

target_link_libraries(engine PUBLIC
    kryga::utils
    kryga::core
    kryga::vulkan_render
    kryga::animation
    kryga::asset_importer
    kryga::render_bridge
    kryga::native
    kryga::vfs
    kryga::asset_converter
    kryga::project_paths

    kryga::textedit_unofficial
    kryga::imguizmo_unofficial
    kryga::imguifiledialog_unofficial

    ${KRG_ACTIVE_MODULES_TARGETS}

    sdl2

    TracyClient

    CLI11::CLI11
)

if(KRG_ENABLE_SYNC_SERVICE)
    target_link_libraries(engine PUBLIC
        boost_beast Boost::url Boost::process Boost::filesystem)
endif()

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

if(ANDROID)
    # On Android the "app" is libmain.so — SDL2's SDLActivity loads this and
    # calls into the main() symbol which SDL renames to SDL_main via a macro.
    add_library(engine_app SHARED "private/src/app/main.cpp")
    set_target_properties(engine_app PROPERTIES OUTPUT_NAME "main")
    target_link_libraries(engine_app PRIVATE log android)
else()
    add_executable(engine_app "private/src/app/main.cpp")
    kryga_finalize_executable(engine_app)
endif()

target_include_directories(engine_app
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(engine_app PRIVATE
    engine
)

add_subdirectory(private/tests/engine_regression)
