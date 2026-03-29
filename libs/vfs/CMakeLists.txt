file(GLOB LIB_SRC "public/include/vfs/*.h" "private/src/*.cpp")

add_library(vfs STATIC
   ${LIB_SRC}
)

target_link_libraries(vfs
    kryga::utils
    kryga::global_state

    spdlog
)
kryga_finalize_library(vfs)

add_subdirectory(private/tests)
