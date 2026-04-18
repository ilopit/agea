FetchContent_GetProperties(vma)

add_library(vma_unofficial STATIC vma.cpp)

target_include_directories(vma_unofficial PUBLIC "${vma_SOURCE_DIR}/include" )
target_link_libraries(vma_unofficial PUBLIC Vulkan::Vulkan)

# On Android, libvulkan.so exports only Vulkan 1.0 symbols. Everything beyond
# that (buffer_device_address, maintenance4, etc.) must be resolved at runtime
# via vkGetDeviceProcAddr. Switch VMA to dynamic function loading.
if(ANDROID)
    target_compile_definitions(vma_unofficial PUBLIC
        VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_DYNAMIC_VULKAN_FUNCTIONS=1)
endif()

add_library(kryga::vma_unofficial ALIAS vma_unofficial)