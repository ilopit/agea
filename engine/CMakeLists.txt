message(STATUS "AGEA_ACTIVE_MODULES_TARGETS : ${AGEA_ACTIVE_MODULES_TARGETS}")

set(ar_folder ${CMAKE_BINARY_DIR}/agea_generated/engine)
set(modules_file ${ar_folder}/active_modules.cpp)

file(GLOB ENGINE_SRC "public/include/engine/*.h" "private/src/*.cpp")

add_library(engine STATIC
   ${ENGINE_SRC}
   ${modules_file}
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
    
)


agea_finalize_library(engine)

add_executable(engine_app "private/src/app/main.cpp")

target_include_directories(engine_app 
    PUBLIC 
        "${CMAKE_CURRENT_SOURCE_DIR}/engine" 
    PUBLIC 
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(engine_app
    engine
)

agea_finalize_executable(engine_app)