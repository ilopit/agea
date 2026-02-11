file(GLOB LIB_SRC "public/include/animation/*.h" "private/src/*.cpp")

add_library(animation STATIC
   ${LIB_SRC}
)

target_link_libraries(animation PUBLIC
    kryga::utils
    kryga::gpu_types
    kryga::glm_unofficial
    kryga::cgltf
    kryga::vulkan_render
    kryga::global_state
    ozz_animation
    ozz_base
)

kryga_finalize_library(animation)
