set(CMAKE_CXX_STANDARD 17)

file(GLOB LIB_SRC "include/serialization/*.h" "src/*.cpp")

add_library(serialization STATIC
   ${LIB_SRC}
)
target_link_libraries(serialization 
    PUBLIC
    agea::utils 
    yaml-cpp
)
agea_finalize_library(serialization)