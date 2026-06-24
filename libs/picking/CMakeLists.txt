file(GLOB LIB_SRC "public/include/picking/*.h" "private/src/*.cpp")

add_library(picking STATIC
   ${LIB_SRC}
)

target_link_libraries(picking PUBLIC
    kryga::core
    kryga::global_state
    kryga::native
    kryga::packages.root.model
    kryga::packages.root.model
)

kryga_finalize_library(picking)
