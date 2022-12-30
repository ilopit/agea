#pragma once

#include <vk_mem_alloc.h>

#include <vector>
#include <functional>

namespace agea
{
namespace render
{

constexpr size_t DESCRIPTORS_SETS_COUNT = 4UL;

using vk_device_provider = std::function<VkDevice()>;
using vma_allocator_provider = std::function<VmaAllocator()>;

}  // namespace render
}  // namespace agea