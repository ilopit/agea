file(GLOB LIB_SRC "public/include/vfx/*.h" "private/src/*.cpp")

add_library(vfx STATIC
   ${LIB_SRC}
)

target_link_libraries(vfx PUBLIC
    kryga::utils
    kryga::glm_unofficial
)

kryga_finalize_library(vfx)
