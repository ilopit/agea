file(GLOB LIB_SRC
    "public/include/cook/*.h"
    "private/src/*.cpp"
    "private/src/*.h"
)

add_library(cook STATIC ${LIB_SRC})

target_link_libraries(cook PUBLIC
    kryga::utils
    kryga::serialization
    kryga::vfs
)

kryga_finalize_library(cook)
