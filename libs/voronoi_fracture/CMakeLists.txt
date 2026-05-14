file(GLOB LIB_SRC
    "public/include/voronoi_fracture/*.h"
    "private/src/*.cpp")

add_library(voronoi_fracture STATIC
    ${LIB_SRC}
)

target_link_libraries(voronoi_fracture
    PUBLIC kryga::gpu_types kryga::glm_unofficial
    PRIVATE voro++ manifold
)

kryga_finalize_library(voronoi_fracture)
