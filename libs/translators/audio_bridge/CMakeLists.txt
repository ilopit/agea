file(GLOB LIB_SRC "public/include/audio_bridge/*.h" "private/src/*.cpp")

add_library(audio_bridge STATIC
   ${LIB_SRC}
)

target_link_libraries(audio_bridge

    kryga::packages.root.model
    kryga::core
    kryga::audio
    kryga::global_state
    kryga::utils
    kryga::glm_unofficial
)

kryga_finalize_library(audio_bridge)
