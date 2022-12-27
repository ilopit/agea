file(GLOB EXEC_SRC "src/*.cpp")

add_executable(engine_app
   ${EXEC_SRC}
)
target_include_directories(engine_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/engine" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

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