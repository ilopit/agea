set(CMAKE_CXX_STANDARD 17)

file(GLOB MODEL_COMPONENTS_SRC
    "model/components/*.h"
    "model/components/*.cpp"
)

file(GLOB MODEL_RENDERING_SRC
    "model/rendering/*.h"
    "model/rendering/*.cpp"
)

file(GLOB MODEL_SRC
    "model/*.h"
    "model/*.cpp"
)

file(GLOB CORE_SRC
    "*.h"
    "*.cpp"
)

file(GLOB REFLECTION_SRC
    "reflection/*.h"
    "reflection/*.cpp"
)

file(GLOB SERIALIZATION_SRC
    "serialization/*.h"
    "serialization/*.cpp"
)

file(GLOB UTILS_SRC
    "utils/*.h"
    "utils/*.cpp"
)

file(GLOB UI_SRC
    "ui/*.h"
    "ui/*.cpp"
)

file(GLOB VULKAN_RENDER_SRC
    "vulkan_render/*.h"
    "vulkan_render/*.cpp"
)

file(GLOB VULKAN_RENDER_LOADERS_SRC
    "vulkan_render/data_loaders/*.h"
    "vulkan_render/data_loaders/*.cpp"
)


source_group("core"             FILES ${CORE_SRC})
source_group("model"            FILES ${MODEL_SRC})
source_group("model/components" FILES ${MODEL_COMPONENTS_SRC})
source_group("model/rendering"  FILES ${MODEL_RENDERING_SRC})
source_group("reflection"       FILES ${REFLECTION_SRC})
source_group("serialization"    FILES ${SERIALIZATION_SRC})
source_group("vulkan_render"    FILES ${VULKAN_RENDER_SRC})
source_group("ui"               FILES ${UI_SRC})
source_group("utils"            FILES ${UTILS_SRC})

source_group("vulkan_render/data_loaders"    FILES ${VULKAN_RENDER_LOADERS_SRC})

add_executable (agea
    "main.cpp"

    ${CORE_SRC}
    ${MODEL_SRC}
    ${MODEL_COMPONENTS_SRC}
    ${MODEL_RENDERING_SRC}
    ${REFLECTION_SRC}
    ${SERIALIZATION_SRC}
    ${VULKAN_RENDER_SRC}
    ${VULKAN_RENDER_LOADERS_SRC}
    ${UTILS_SRC}
    ${UI_SRC}
 )


target_include_directories(agea PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

set_target_properties(agea PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}/project_Debug/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}/project_Release/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/project_RelWithDebInfo/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}/project_MinSizeRel/bin"

    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/bin"

    FOLDER "agea"
)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/assets
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/assets)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/shaders
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/shaders)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/configs
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/configs)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/levels
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/levels)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/objects
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/objects)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${PROJECT_SOURCE_DIR}/resources/components
    ${CMAKE_BINARY_DIR}/project_${CMAKE_BUILD_TYPE}/components)

target_link_libraries(agea 
    vkbootstrap
    vma
    glm
    tinyobjloader 
    imgui 
    stb_image 
    spirv_reflect 
    jsoncpp_static 
    Vulkan::Vulkan 
    sdl2)
