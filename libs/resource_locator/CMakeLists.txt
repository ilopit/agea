file(GLOB LIB_SRC "public/include/resource_locator/*.h" "private/src/*.cpp")

file(GLOB_RECURSE  FSHADERS_SRC "${PROJECT_SOURCE_DIR}/resources/packages/*.frag")
file(GLOB_RECURSE  VSHADERS_SRC "${PROJECT_SOURCE_DIR}/resources/packages/*.vert")

source_group("shaders" FILES  ${FSHADERS_SRC} ${VSHADERS_SRC})

add_library(resource_locator STATIC
   ${LIB_SRC}
   ${FSHADERS_SRC} 
   ${VSHADERS_SRC}
)

target_link_libraries(resource_locator
    agea::utils

    spdlog 
)
agea_finalize_library(resource_locator)