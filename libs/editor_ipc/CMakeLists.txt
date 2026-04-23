# editor_ipc is the static library; shmdump_main.cpp is a separate
# executable target, listed explicitly so we don't pull main() into the lib.
set(LIB_SRC
    public/include/editor_ipc/frame_protocol.h
    public/include/editor_ipc/shared_memory.h
    public/include/editor_ipc/named_event.h
    public/include/editor_ipc/input_reader.h
    public/include/editor_ipc/frame_publisher.h
    private/src/shared_memory.cpp
    private/src/named_event.cpp
    private/src/input_reader.cpp
    private/src/frame_publisher.cpp
)

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

# Diagnostic CLI: attaches to a running region, prints header atomics,
# optionally saves the most recent ready slot to PNG.
add_executable(editor_ipc_shmdump private/src/shmdump_main.cpp)
target_link_libraries(editor_ipc_shmdump PRIVATE
    kryga::editor_ipc
    kryga::render_utils
)
kryga_finalize_executable(editor_ipc_shmdump)
