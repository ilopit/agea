file(GLOB LIB_HEADERS "public/include/gpu_types/*.h")

add_library(gpu_types INTERFACE)

target_include_directories(gpu_types INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/public/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(gpu_types INTERFACE agea::glm_unofficial)

add_library(agea::gpu_types ALIAS gpu_types)
