file(GLOB LIB_SRC "public/include/serialization/*.h" "private/src/*.cpp")

add_library(serialization STATIC
   ${LIB_SRC}
)
target_link_libraries(serialization 
    PUBLIC
    kryga::utils 
    yaml-cpp
)
kryga_finalize_library(serialization)