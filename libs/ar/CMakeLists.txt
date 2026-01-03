file(GLOB LIB_SRC "include/ar/*.h")

add_library(ar INTERFACE
   ${LIB_SRC}
)

kryga_finalize_interface_library(ar)