file(GLOB LIB_SRC "public/include/physics_bridge/*.h" "private/src/*.cpp")

add_library(physics_bridge STATIC
   ${LIB_SRC}
)

target_link_libraries(physics_bridge

    kryga::packages.root.model
    kryga::utils
    kryga::physics
)

kryga_finalize_library(physics_bridge)
