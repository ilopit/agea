file(GLOB LIB_SRC "public/include/render_utils/*.h" "private/src/*.cpp")

add_library(render_utils STATIC
   ${LIB_SRC}
)

target_link_libraries(render_utils PUBLIC
    kryga::glm_unofficial
    kryga::gpu_types
)

kryga_finalize_library(render_utils)

add_subdirectory(private/tests)
