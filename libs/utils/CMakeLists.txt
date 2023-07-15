file(GLOB LIB_SRC "public/include/utils/*.h" "private/src/*.cpp")

add_library(utils STATIC
   ${LIB_SRC}
)
target_link_libraries(utils spdlog)
agea_finalize_library(utils)

add_subdirectory(private/tests)