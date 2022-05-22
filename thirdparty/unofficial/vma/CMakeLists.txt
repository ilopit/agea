
add_library(vma_unofficial STATIC vma.cpp)

target_include_directories(vma_unofficial PUBLIC "${PROJECT_SOURCE_DIR}/thirdparty/upstream/vma/src" )

target_link_libraries(vma_unofficial PUBLIC Vulkan::Vulkan)