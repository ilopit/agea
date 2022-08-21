file(GLOB LIB_SRC "include/arl/*.h")

add_library(arl INTERFACE
   ${LIB_SRC}
)

agea_finalize_interface_library(arl)