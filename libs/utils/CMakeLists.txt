

set(CMAKE_CXX_STANDARD 17)

file(GLOB LIB_SRC "include/utils/*.h" "src/*.cpp")

add_library(utils STATIC
   ${LIB_SRC}
)
target_link_libraries(utils spdlog)
agea_finalize_library(utils)

add_subdirectory(tests)