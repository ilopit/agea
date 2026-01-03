file(GLOB LIB_SRC "public/include/global_state/*.h" "private/src/*.cpp")

add_library(global_state STATIC
   ${LIB_SRC}
)

target_link_libraries(global_state
    kryga::utils
)

kryga_finalize_library(global_state)