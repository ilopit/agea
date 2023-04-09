
macro(agea_ide_path)

    get_filename_component(PARENT_DIR_1 ${PROJECT_SOURCE_DIR} DIRECTORY)
    file(RELATIVE_PATH rel ${PARENT_DIR_1} ${CMAKE_CURRENT_LIST_DIR})
    set_target_properties(${ARGV0} PROPERTIES FOLDER ${rel})

endmacro()

macro(agea_finalize_library)

    agea_ide_path(${ARGV0})

    target_include_directories(${ARGV0} 
        PUBLIC 
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}")

    add_library(agea::${ARGV0} ALIAS ${ARGV0})
endmacro()

macro(agea_finalize_interface_library)

    agea_ide_path(${ARGV0})

    target_include_directories(${ARGV0} 
        INTERFACE 
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}")

    add_library(agea::${ARGV0} ALIAS ${ARGV0})
endmacro()

macro(agea_finalize_executable)

    agea_ide_path(${ARGV0})

    set_target_properties(${ARGV0}  PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}/project_Debug/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}/project_Release/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/project_RelWithDebInfo/bin"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}/project_MinSizeRel/bin"

        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/bin"
    )

endmacro()

macro(agea_ar_target)

    set(full_name ${ARGV0})

    add_custom_command(
        TARGET ${full_name}
        PRE_BUILD
        COMMAND python ${PROJECT_SOURCE_DIR}/tools/argen.py
                    ${PROJECT_SOURCE_DIR}/modules/${ARGV1}/ar/config 
                    ${PROJECT_SOURCE_DIR}/modules/${ARGV1}
                    ${CMAKE_BINARY_DIR}/agea_generated
                    ${ARGV1}
                    " "
                    ${ARGV2})

    set(ar_folder ${CMAKE_BINARY_DIR}/agea_generated/${ARGV1})
    set(ar_file ${ar_folder}/${ARGV1}.ar.cpp)

    agea_ide_path(${full_name})

    if(EXISTS ${ar_file})
        message("File already exists, skipping")
    else()

        if(NOT EXISTS)
        file(MAKE_DIRECTORY ${ar_folder})
        endif()

        write_file(${ar_file} "//ar file do not modify")
    endif()
endmacro()