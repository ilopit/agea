file(GLOB LIB_SRC "public/include/audio/*.h" "private/src/*.cpp")

add_library(audio STATIC
   ${LIB_SRC}
)

target_link_libraries(audio PUBLIC
    kryga::utils
    kryga::glm_unofficial
    kryga::global_state
    kryga::core
    kryga::miniaudio
)

kryga_finalize_library(audio)
