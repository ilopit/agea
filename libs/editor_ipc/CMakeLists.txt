file(GLOB LIB_SRC "public/include/editor_ipc/*.h" "private/src/*.cpp")

add_library(editor_ipc STATIC
   ${LIB_SRC}
)

target_link_libraries(editor_ipc PUBLIC
    kryga::utils
    kryga::vulkan_render
    kryga::vma_unofficial
    Vulkan::Vulkan
)

if(UNIX)
    # shm_open / mmap live in librt on glibc systems; macOS exposes them in libc.
    if(NOT APPLE)
        target_link_libraries(editor_ipc PUBLIC rt)
    endif()
endif()

kryga_finalize_library(editor_ipc)
