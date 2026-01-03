file(GLOB LIB_SRC "public/include/native/*.h" "private/src/*.cpp")

add_library(native STATIC
   ${LIB_SRC}
)

target_link_libraries(native
    kryga::utils
    sdl2
    kryga::stb_unofficial
    kryga::resource_locator
)

kryga_finalize_library(native)