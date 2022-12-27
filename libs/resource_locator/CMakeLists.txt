file(GLOB LIB_SRC "include/resource_locator/*.h" "src/*.cpp")

add_library(resource_locator STATIC
   ${LIB_SRC}
)
target_link_libraries(resource_locator
    agea::utils

    spdlog 
)
agea_finalize_library(resource_locator)