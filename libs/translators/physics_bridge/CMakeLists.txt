file(GLOB LIB_SRC "public/include/physics_bridge/*.h" "private/src/*.cpp")

add_library(physics_bridge STATIC
   ${LIB_SRC}
)

target_link_libraries(physics_bridge

    kryga::core
    kryga::physics
    kryga::global_state
    kryga::utils
    kryga::glm_unofficial
    kryga::packages.root.model
)

kryga_finalize_library(physics_bridge)
