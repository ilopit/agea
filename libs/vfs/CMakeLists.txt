file(GLOB LIB_SRC "public/include/vfs/*.h" "private/src/*.cpp")

add_library(vfs STATIC
   ${LIB_SRC}
)

target_link_libraries(vfs
    kryga::utils
    kryga::global_state

    spdlog

    Boost::filesystem
)

if(ANDROID)
    # AAssetManager comes from libandroid.so; need -llog for __android_log_print
    # which spdlog/Android sinks may pull in via utils.
    target_link_libraries(vfs android log)
endif()

kryga_finalize_library(vfs)

if(NOT ANDROID)
    add_subdirectory(private/tests)
endif()
