#pragma once

#include "vulkan_render/vulkan_types.h"

#include <vector>
#include <array>
#include <unordered_map>

namespace agea
{
namespace vk_utils
{
class descriptor_allocator
{
public:
    struct pool_sizes
    {
        std::vector<std::pair<VkDescriptorType, float>> sizes = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f}};
    };

    void
    reset_pools();
    bool
    allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);

    void
    init(VkDevice newDevice);

    void
    cleanup();

    VkDevice device;

private:
    VkDescriptorPool
    grab_pool();

    VkDescriptorPool currentPool{VK_NULL_HANDLE};
    pool_sizes descriptorSizes;
    std::vector<VkDescriptorPool> usedPools;
    std::vector<VkDescriptorPool> freePools;
};

class descriptor_layout_cache
{
public:
    void
    init(VkDevice newDevice);
    void
    cleanup();

    VkDescriptorSetLayout
    create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info);

    struct descriptor_layout_info
    {
        // good idea to turn this into a inlined array
        std::vector<VkDescriptorSetLayoutBinding> bindings;

        bool
        operator==(const descriptor_layout_info& other) const;

        size_t
        hash() const;
    };

private:
    struct descriptor_layout_hash
    {
        std::size_t
        operator()(const descriptor_layout_info& k) const
        {
            return k.hash();
        }
    };

    std::unordered_map<descriptor_layout_info, VkDescriptorSetLayout, descriptor_layout_hash>
        m_layout_cache;
    VkDevice device;
};

class descriptor_builder
{
public:
    static descriptor_builder
    begin(descriptor_layout_cache* layoutCache, descriptor_allocator* allocator);

    descriptor_builder&
    bind_buffer(uint32_t binding,
                VkDescriptorBufferInfo* bufferInfo,
                VkDescriptorType type,
                VkShaderStageFlags stageFlags);

    descriptor_builder&
    bind_image(uint32_t binding,
               VkDescriptorImageInfo* imageInfo,
               VkDescriptorType type,
               VkShaderStageFlags stageFlags);

    bool
    build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
    bool
    build(VkDescriptorSet& set);

private:
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    descriptor_layout_cache* cache;
    descriptor_allocator* alloc;
};
}  // namespace vk_utils

}  // namespace agea
