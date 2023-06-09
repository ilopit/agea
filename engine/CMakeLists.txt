message(STATUS "AGEA_ACTIVE_MODULES_TARGETS : ${AGEA_ACTIVE_MODULES_TARGETS}")

set(ar_folder ${CMAKE_BINARY_DIR}/agea_generated/engine)
set(modules_file ${ar_folder}/active_modules.cpp)

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

add_executable(engine_app "src/app/main.cpp")

target_include_directories(engine_app 
    PUBLIC 
        "${CMAKE_CURRENT_SOURCE_DIR}/engine" 
    PUBLIC 
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(engine_app
    engine
)

set_target_properties(engine_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}/project_Debug/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}/project_Release/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/project_RelWithDebInfo/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}/project_MinSizeRel/bin"

    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/bin"
)

get_filename_component(PARENT_DIR_1 ${PROJECT_SOURCE_DIR} DIRECTORY)
get_filename_component(PARENT_DIR_2 ${CMAKE_CURRENT_LIST_DIR} DIRECTORY)
file(RELATIVE_PATH rel ${PARENT_DIR_1} ${PARENT_DIR_2})
set_target_properties(engine_app PROPERTIES FOLDER ${rel})