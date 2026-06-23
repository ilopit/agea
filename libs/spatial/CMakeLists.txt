file(GLOB LIB_SRC "public/include/spatial/*.h" "private/src/*.cpp")

add_library(spatial STATIC
   ${LIB_SRC}
)

target_link_libraries(spatial PUBLIC
    kryga::glm_unofficial
)

kryga_finalize_library(spatial)
