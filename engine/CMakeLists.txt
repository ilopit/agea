file(GLOB ENGINE_SRC           "public/include/engine/*.h"    "private/src/*.cpp")
file(GLOB ENGINE_UI_SRC        "public/include/engine/ui/*.h" "private/src/ui/*.cpp")
file(GLOB ENGINE_GENERATED_SRC "${CMAKE_BINARY_DIR}/agea_generated/engine/*.cpp")

add_library(engine STATIC
   ${ENGINE_SRC}
   ${ENGINE_UI_SRC}
   ${ENGINE_GENERATED_SRC}
)

target_link_libraries(engine PUBLIC
    agea::utils
    agea::core
    agea::vulkan_render
    agea::asset_importer
    agea::render_bridge
    agea::native

    agea::textedit_unofficial

    ${AGEA_ACTIVE_MODULES_TARGETS}

    boost_beast
    Boost::url

    sdl2
)

target_include_directories(engine 
    PUBLIC 
        ${CMAKE_BINARY_DIR}/agea_generated
    PRIVATE 
        "${CMAKE_CURRENT_SOURCE_DIR}/private/include" 
)

agea_finalize_library(engine)

add_executable(engine_app "private/src/app/main.cpp")

target_include_directories(engine_app 
    PUBLIC 
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(engine_app
    engine
)

agea_finalize_executable(engine_app)