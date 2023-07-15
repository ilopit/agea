file(GLOB LIB_SRC "public/include/serialization/*.h" "private/src/*.cpp")

add_library(serialization STATIC
   ${LIB_SRC}
)
target_link_libraries(serialization 
    PUBLIC
    agea::utils 
    yaml-cpp
)
agea_finalize_library(serialization)