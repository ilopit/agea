FetchContent_GetProperties(vma)

add_library(vma_unofficial STATIC vma.cpp)

target_include_directories(vma_unofficial PUBLIC "${vma_SOURCE_DIR}/include" )
target_link_libraries(vma_unofficial PUBLIC Vulkan::Vulkan)

add_library(kryga::vma_unofficial ALIAS vma_unofficial)