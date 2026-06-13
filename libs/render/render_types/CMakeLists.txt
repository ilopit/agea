add_library(render_types INTERFACE)

# Header-only, Vulkan-free: typed render-resource handles + slot allocator/pool.
# Linkable by BOTH the model (packages) and the render lib without pulling Vulkan.
target_link_libraries(render_types INTERFACE
    kryga::utils)

kryga_finalize_interface_library(render_types)

add_subdirectory(private/tests)
add_subdirectory(private/benchmarks)
