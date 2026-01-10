file(GLOB LIB_SRC "public/include/gpu_types/*.h" "private/src/*.cpp")

add_library(gpu_types STATIC
   ${LIB_SRC}
)

target_link_libraries(gpu_types PUBLIC
    kryga::glm_unofficial
    kryga::utils
)

kryga_finalize_library(gpu_types)
