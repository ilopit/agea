macro(agea_finalize_library)

    get_filename_component(PARENT_DIR_1 ${PROJECT_SOURCE_DIR} DIRECTORY)
    file(RELATIVE_PATH rel ${PARENT_DIR_1} ${CMAKE_CURRENT_LIST_DIR})
    set_target_properties(${ARGV0} PROPERTIES FOLDER ${rel})

    target_include_directories(${ARGV0} 
        PUBLIC 
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}")

    add_library(agea::${ARGV0} ALIAS ${ARGV0})
endmacro()

macro(agea_finalize_interface_library)

    get_filename_component(PARENT_DIR_1 ${PROJECT_SOURCE_DIR} DIRECTORY)
    file(RELATIVE_PATH rel ${PARENT_DIR_1} ${CMAKE_CURRENT_LIST_DIR})
    set_target_properties(${ARGV0} PROPERTIES FOLDER ${rel})

    target_include_directories(${ARGV0} 
        INTERFACE 
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}")

    add_library(agea::${ARGV0} ALIAS ${ARGV0})
endmacro()

macro(agea_finalize_executable)

    get_filename_component(PARENT_DIR_1 ${PROJECT_SOURCE_DIR} DIRECTORY)
    file(RELATIVE_PATH rel ${PARENT_DIR_1} ${CMAKE_CURRENT_LIST_DIR})
    set_target_properties(${ARGV0} PROPERTIES FOLDER ${rel})

    set_target_properties(${ARGV0}  PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}/project_Debug/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}/project_Release/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/project_RelWithDebInfo/bin"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}/project_MinSizeRel/bin"

        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/bin"
    )

endmacro()