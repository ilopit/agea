#include "vulkan_render/vk_descriptors.h"

#include "vulkan_render/vulkan_render_device.h"

#include <algorithm>

namespace agea::render::vk_utils
{

namespace
{
const uint32_t SETS_COUNT = 64;
VkDescriptorPool
create_pool(const descriptor_allocator::pool_sizes_mapping& pool_sizes,
            uint32_t sets_count,
            VkDescriptorPoolCreateFlags flags)
{
    std::vector<VkDescriptorPoolSize> sizes;
    sizes.reserve(pool_sizes.size());
    for (const auto& [f, s] : pool_sizes)
    {
        sizes.push_back({f, uint32_t(s * sets_count)});
    }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = flags;
    pool_info.maxSets = sets_count;
    pool_info.poolSizeCount = (uint32_t)sizes.size();
    pool_info.pPoolSizes = sizes.data();

    auto device = glob::render_device::getr().vk_device();
    VkDescriptorPool pool;
    vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);

    return pool;
}
}  // namespace

void
descriptor_allocator::reset_pools()
{
    auto device = glob::render_device::getr().vk_device();

    for (auto p : m_used_pools)
    {
        vkResetDescriptorPool(device, p, 0);
    }

    m_free_pools = m_used_pools;
    m_used_pools.clear();
    m_current_pool = VK_NULL_HANDLE;
}

bool
descriptor_allocator::allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout)
{
    if (m_current_pool == VK_NULL_HANDLE)
    {
        m_current_pool = grab_pool();
        m_used_pools.push_back(m_current_pool);
    }

    VkDescriptorSetAllocateInfo descriptor_ai = {};
    descriptor_ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_ai.pNext = nullptr;

    descriptor_ai.pSetLayouts = &layout;
    descriptor_ai.descriptorPool = m_current_pool;
    descriptor_ai.descriptorSetCount = 1;

    auto device = glob::render_device::getr().vk_device();
    VkResult alloc_result = vkAllocateDescriptorSets(device, &descriptor_ai, set);
    bool is_reallocate_needed = false;

    switch (alloc_result)
    {
    case VK_SUCCESS:
        // all good, return
        return true;

        break;
    case VK_ERROR_FRAGMENTED_POOL:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        // reallocate pool
        is_reallocate_needed = true;
        break;
    default:
        // unrecoverable error
        return false;
    }

    if (is_reallocate_needed)
    {
        // allocate a new pool and retry
        m_current_pool = grab_pool();
        m_used_pools.push_back(m_current_pool);

        alloc_result = vkAllocateDescriptorSets(device, &descriptor_ai, set);

        // if it still fails then we have big issues
        if (alloc_result == VK_SUCCESS)
        {
            return true;
        }
    }

    return false;
}

void
descriptor_allocator::cleanup()
{
    auto device = glob::render_device::getr().vk_device();

    // delete every pool held
    for (auto p : m_free_pools)
    {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    for (auto p : m_used_pools)
    {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
}

VkDescriptorPool
descriptor_allocator::grab_pool()
{
    if (m_free_pools.size() > 0)
    {
        VkDescriptorPool pool = m_free_pools.back();
        m_free_pools.pop_back();
        return pool;
    }
    else
    {
        return create_pool(m_descriptor_sizes, 64, 0);
    }
}

descriptor_allocator::pool_sizes_mapping
descriptor_allocator::get_default_pool_size()
{
    pool_sizes_mapping result = {{VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f},
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

    return result;
}

VkDescriptorSetLayout
descriptor_layout_cache::create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info)
{
    descriptor_layout_info layout_info;
    layout_info.bindings.reserve(info->bindingCount);
    bool is_sorted = true;
    int last_binding = -1;
    for (int i = 0; i < (int)info->bindingCount; i++)
    {
        layout_info.bindings.push_back(info->pBindings[i]);

        // check that the bindings are in strict increasing order
        if ((int)info->pBindings[i].binding > last_binding)
        {
            last_binding = info->pBindings[i].binding;
        }
        else
        {
            is_sorted = false;
        }
    }
    if (!is_sorted)
    {
        std::sort(layout_info.bindings.begin(), layout_info.bindings.end(),
                  [](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b)
                  { return a.binding < b.binding; });
    }

    auto it = m_layout_cache.find(layout_info);
    if (it != m_layout_cache.end())
    {
        return (*it).second;
    }
    else
    {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        auto device = glob::render_device::getr().vk_device();
        vkCreateDescriptorSetLayout(device, info, nullptr, &layout);

        m_layout_cache[layout_info] = layout;
        return layout;
    }
}

void
descriptor_layout_cache::cleanup()
{
    for (const auto& pair : m_layout_cache)
    {
        auto device = glob::render_device::getr().vk_device();
        vkDestroyDescriptorSetLayout(device, pair.second, nullptr);
    }
}

descriptor_builder
descriptor_builder::begin(descriptor_layout_cache* layoutCache, descriptor_allocator* allocator)
{
    descriptor_builder builder;

    builder.m_cache = layoutCache;
    builder.m_alloc = allocator;
    return builder;
}

descriptor_builder&
descriptor_builder::bind_buffer(uint32_t binding,
                                VkDescriptorBufferInfo* buffer_info,
                                VkDescriptorType type,
                                VkShaderStageFlags stage_flags)
{
    VkDescriptorSetLayoutBinding new_binding{};

    new_binding.descriptorCount = 1;
    new_binding.descriptorType = type;
    new_binding.pImmutableSamplers = nullptr;
    new_binding.stageFlags = stage_flags;
    new_binding.binding = binding;

    m_bindings.push_back(new_binding);

    VkWriteDescriptorSet new_write{};
    new_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    new_write.pNext = nullptr;

    new_write.descriptorCount = 1;
    new_write.descriptorType = type;
    new_write.pBufferInfo = buffer_info;
    new_write.dstBinding = binding;

    m_writes.push_back(new_write);

    return *this;
}

descriptor_builder&
descriptor_builder::bind_image(uint32_t binding,
                               VkDescriptorImageInfo* image_info,
                               VkDescriptorType type,
                               VkShaderStageFlags stage_flags)
{
    VkDescriptorSetLayoutBinding new_binding{};

    new_binding.descriptorCount = 1;
    new_binding.descriptorType = type;
    new_binding.pImmutableSamplers = nullptr;
    new_binding.stageFlags = stage_flags;
    new_binding.binding = binding;

    m_bindings.push_back(new_binding);

    VkWriteDescriptorSet new_write{};
    new_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    new_write.pNext = nullptr;

    new_write.descriptorCount = 1;
    new_write.descriptorType = type;
    new_write.pImageInfo = image_info;
    new_write.dstBinding = binding;

    m_writes.push_back(new_write);
    return *this;
}

bool
descriptor_builder::build(VkDescriptorSet& set, VkDescriptorSetLayout& layout)
{
    // build layout first
    VkDescriptorSetLayoutCreateInfo layout_set_ci{};
    layout_set_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_set_ci.pNext = nullptr;

    layout_set_ci.pBindings = m_bindings.data();
    layout_set_ci.bindingCount = (uint32_t)m_bindings.size();

    layout = m_cache->create_descriptor_layout(&layout_set_ci);

    // allocate descriptor
    bool success = m_alloc->allocate(&set, layout);
    if (!success)
    {
        return false;
    };

    for (VkWriteDescriptorSet& w : m_writes)
    {
        w.dstSet = set;
    }

    auto device = glob::render_device::getr().vk_device();
    vkUpdateDescriptorSets(device, (uint32_t)m_writes.size(), m_writes.data(), 0, nullptr);

    return true;
}

bool
descriptor_builder::build(VkDescriptorSet& set)
{
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    return build(set, layout);
}

bool
descriptor_layout_cache::descriptor_layout_info::operator==(
    const descriptor_layout_info& other) const
{
    if (other.bindings.size() != bindings.size())
    {
        return false;
    }
    else
    {
        // compare each of the bindings is the same. Bindings are sorted so they will match
        for (int i = 0; i < bindings.size(); i++)
        {
            if (other.bindings[i].binding != bindings[i].binding)
            {
                return false;
            }
            if (other.bindings[i].descriptorType != bindings[i].descriptorType)
            {
                return false;
            }
            if (other.bindings[i].descriptorCount != bindings[i].descriptorCount)
            {
                return false;
            }
            if (other.bindings[i].stageFlags != bindings[i].stageFlags)
            {
                return false;
            }
        }
        return true;
    }
}

size_t
descriptor_layout_cache::descriptor_layout_info::hash() const
{
    size_t result = std::hash<size_t>()(bindings.size());

    for (const VkDescriptorSetLayoutBinding& b : bindings)
    {
        // pack the binding data into a single int64. Not fully correct but its ok
        size_t binding_hash =
            b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;

        // shuffle the packed binding data and xor it with the main hash
        result ^= std::hash<size_t>()(binding_hash);
    }

    return result;
}

}  // namespace agea::render::vk_utils
