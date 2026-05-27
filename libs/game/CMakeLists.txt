file(GLOB LIB_SRC "public/include/game/*.h" "private/src/*.cpp")

add_library(game STATIC
    ${LIB_SRC}
)

target_link_libraries(game PUBLIC
    kryga::utils
    kryga::global_state
    kryga::core
)

kryga_finalize_library(game)
