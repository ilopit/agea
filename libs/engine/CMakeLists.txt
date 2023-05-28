message(STATUS "AGEA_ACTIVE_MODULES_TARGETS : ${AGEA_ACTIVE_MODULES_TARGETS}")

set(ar_folder ${CMAKE_BINARY_DIR}/agea_generated)
set(modules_file ${ar_folder}/active_modules.cpp)



if(EXISTS ${modules_file})
    message("File already exists, skipping")
else()

    file(MAKE_DIRECTORY ${ar_folder})
    write_file(${modules_file} "#include \"engine/active_modules.h\"")
endif()




file(GLOB ENGINE_SRC "include/engine/*.h" "src/*.cpp")

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

    ${AGEA_ACTIVE_MODULES_TARGETS}

    sdl2
)

agea_finalize_library(engine)