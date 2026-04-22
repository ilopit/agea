file(GLOB LIB_PUBLIC_SRC  "public/include/physics/*.h")
file(GLOB LIB_PRIVATE_SRC "private/include/physics_internal/*.h"
                          "private/src/*.cpp")

add_library(physics STATIC
    ${LIB_PUBLIC_SRC}
    ${LIB_PRIVATE_SRC}
)

target_link_libraries(physics
    PUBLIC
        kryga::glm_unofficial
        kryga::utils
        kryga::global_state
    PRIVATE
        Jolt
)

kryga_finalize_library(physics)
