file(GLOB LIB_SRC "public/include/shader_system/*.h" "private/src/*.cpp")

add_library(shader_system STATIC
   ${LIB_SRC}
)

target_link_libraries(shader_system PUBLIC
    kryga::error_handling
    kryga::global_state
    kryga::resource_locator
    kryga::utils
)

kryga_finalize_library(shader_system)

add_subdirectory(private/tests)
