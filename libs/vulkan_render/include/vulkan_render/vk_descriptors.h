#pragma once

#include <vector>
#include <array>
#include <unordered_map>

#include <vulkan/vulkan.h>

namespace agea
{
namespace render
{
namespace vk_utils
{
class descriptor_allocator
{
public:
    using pool_sizes_mapping = std::vector<std::pair<VkDescriptorType, float>>;

    static std::vector<std::pair<VkDescriptorType, float>>
    get_default_pool_size();

    void
    reset_pools();

    bool
    allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);

    void
    cleanup();

private:
    VkDescriptorPool
    grab_pool();

    VkDescriptorPool m_current_pool = VK_NULL_HANDLE;
    pool_sizes_mapping m_descriptor_sizes;
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
               uint32_t binding_count,
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
