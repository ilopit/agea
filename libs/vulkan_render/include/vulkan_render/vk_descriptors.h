#pragma once

#include "vulkan_render/types/vulkan_types.h"

#include <vector>
#include <array>
#include <unordered_map>

namespace agea
{
namespace render
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
    init(VkDevice new_device);

    void
    cleanup();

    VkDevice
    device()
    {
        return m_device;
    }

private:
    VkDescriptorPool
    grab_pool();

    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorPool m_current_pool = VK_NULL_HANDLE;
    pool_sizes m_descriptor_sizes;
    std::vector<VkDescriptorPool> m_used_pools;
    std::vector<VkDescriptorPool> m_free_pools;
};

class descriptor_layout_cache
{
public:
    struct descriptor_layout_info
    {
        bool
        operator==(const descriptor_layout_info& other) const;

        size_t
        hash() const;

        std::vector<VkDescriptorSetLayoutBinding> bindings;
    };

    void
    init(VkDevice new_device);
    void
    cleanup();

    VkDescriptorSetLayout
    create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info);

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
    VkDevice m_device;
};

class descriptor_builder
{
public:
    static descriptor_builder
    begin(descriptor_layout_cache* layoutCache, descriptor_allocator* allocator);

    descriptor_builder&
    bind_buffer(uint32_t binding,
                VkDescriptorBufferInfo* buffer_info,
                VkDescriptorType type,
                VkShaderStageFlags stage_flags);

    descriptor_builder&
    bind_image(uint32_t binding,
               VkDescriptorImageInfo* image_info,
               VkDescriptorType type,
               VkShaderStageFlags stage_flags);

    bool
    build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
    bool
    build(VkDescriptorSet& set);

private:
    std::vector<VkWriteDescriptorSet> m_writes;
    std::vector<VkDescriptorSetLayoutBinding> m_bindings;

    descriptor_layout_cache* m_cache = nullptr;
    descriptor_allocator* m_alloc = nullptr;
};
}  // namespace vk_utils
}  // namespace render

}  // namespace agea
