file(GLOB LIB_SRC "public/include/shader_system/*.h" "private/src/*.cpp")

add_library(shader_system STATIC
   ${LIB_SRC}
)

target_link_libraries(shader_system PUBLIC
    kryga::error_handling
    kryga::global_state

    kryga::vfs
    kryga::utils
    kryga::gpu_types
    kryga::glm_unofficial

    Vulkan::Vulkan
    spirv-reflect-static
)

kryga_finalize_library(shader_system)

add_subdirectory(private/tests)
