file(GLOB LIB_SRC "public/include/error_handling/*.h" "private/src/*.cpp")

add_library(error_handling STATIC
   ${LIB_SRC}
)

agea_finalize_library(error_handling)