file(GLOB LIB_SRC "public/include/shader_compiler/*.h" "private/src/*.cpp")

add_library(shader_compiler STATIC
   ${LIB_SRC}
)

target_link_libraries(shader_compiler PUBLIC
    kryga::utils
)

kryga_finalize_library(shader_compiler)

add_subdirectory(private/tests)
