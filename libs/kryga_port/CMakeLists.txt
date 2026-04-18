file(GLOB LIB_SRC "include/kryga_port/*.h")

add_library(kryga_port INTERFACE
   ${LIB_SRC}
)

target_link_libraries(kryga_port INTERFACE spdlog)

kryga_finalize_interface_library(kryga_port)
