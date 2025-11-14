file(GLOB LIB_SRC "public/include/testing/*.h" "private/src/*.cpp")

add_library(testing STATIC
   ${LIB_SRC}
)
target_link_libraries(testing spdlog gtest utils)
agea_finalize_library(testing)