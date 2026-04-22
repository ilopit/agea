file(GLOB LIB_SRC
    "public/include/physics_stub/*.h"
    "private/src/*.cpp")

add_library(physics_stub STATIC
    ${LIB_SRC}
)

target_link_libraries(physics_stub PUBLIC
    kryga::glm_unofficial
)

kryga_finalize_library(physics_stub)
