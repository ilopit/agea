file(GLOB LIB_SRC "include/error_handling/*.h" "src/*.cpp")

add_library(error_handling STATIC
   ${LIB_SRC}
)

agea_finalize_library(error_handling)