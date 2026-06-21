file(GLOB LIB_SRC "public/include/game_session/*.h" "private/src/*.cpp")

add_library(game_session STATIC
   ${LIB_SRC}
)

target_link_libraries(game_session PUBLIC
    kryga::global_state
    kryga::core
    kryga::packages.root.model
    kryga::serialization
    kryga::vfs
    kryga::utils
    kryga::error_handling
)

kryga_finalize_library(game_session)
