file(GLOB LIB_SRC "public/include/native/*.h" "private/src/*.cpp")

add_library(native STATIC
   ${LIB_SRC}
)

target_link_libraries(native
    agea::utils
    sdl2
    agea::stb_unofficial
    agea::resource_locator
)

agea_finalize_library(native)