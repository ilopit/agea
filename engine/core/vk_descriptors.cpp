﻿#include "core/vk_descriptors.h"

#include <algorithm>

namespace agea
{
namespace vk_utils
{
VkDescriptorPool
createPool(VkDevice d,
           const descriptor_allocator::pool_sizes& pool_sizes,
           int count,
           VkDescriptorPoolCreateFlags flags)
{
    std::vector<VkDescriptorPoolSize> sizes;
    sizes.reserve(pool_sizes.sizes.size());
    for (auto sz : pool_sizes.sizes)
    {
        sizes.push_back({sz.first, uint32_t(sz.second * count)});
    }
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = flags;
    pool_info.maxSets = count;
    pool_info.poolSizeCount = (uint32_t)sizes.size();
    pool_info.pPoolSizes = sizes.data();

    VkDescriptorPool descriptorPool;
    vkCreateDescriptorPool(d, &pool_info, nullptr, &descriptorPool);

    return descriptorPool;
}

void
descriptor_allocator::reset_pools()
{
    for (auto p : usedPools)
    {
        vkResetDescriptorPool(device, p, 0);
    }

    freePools = usedPools;
    usedPools.clear();
    currentPool = VK_NULL_HANDLE;
}

bool
descriptor_allocator::allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout)
{
    if (currentPool == VK_NULL_HANDLE)
    {
        currentPool = grab_pool();
        usedPools.push_back(currentPool);
    }

    VkDescriptorSetAllocateInfo dsai = {};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.pNext = nullptr;

    dsai.pSetLayouts = &layout;
    dsai.descriptorPool = currentPool;
    dsai.descriptorSetCount = 1;

    VkResult allocResult = vkAllocateDescriptorSets(device, &dsai, set);
    bool needReallocate = false;

    switch (allocResult)
    {
    case VK_SUCCESS:
        // all good, return
        return true;

        break;
    case VK_ERROR_FRAGMENTED_POOL:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        // reallocate pool
        needReallocate = true;
        break;
    default:
        // unrecoverable error
        return false;
    }

    if (needReallocate)
    {
        // allocate a new pool and retry
        currentPool = grab_pool();
        usedPools.push_back(currentPool);

        allocResult = vkAllocateDescriptorSets(device, &dsai, set);

        // if it still fails then we have big issues
        if (allocResult == VK_SUCCESS)
        {
            return true;
        }
    }

    return false;
}

void
descriptor_allocator::init(VkDevice newDevice)
{
    device = newDevice;
}

void
descriptor_allocator::cleanup()
{
    // delete every pool held
    for (auto p : freePools)
    {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    for (auto p : usedPools)
    {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
}

VkDescriptorPool
descriptor_allocator::grab_pool()
{
    if (freePools.size() > 0)
    {
        VkDescriptorPool pool = freePools.back();
        freePools.pop_back();
        return pool;
    }
    else
    {
        return createPool(device, descriptorSizes, 100000, 0);
    }
}

void
descriptor_layout_cache::init(VkDevice newDevice)
{
    device = newDevice;
}

VkDescriptorSetLayout
descriptor_layout_cache::create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info)
{
    descriptor_layout_info layoutinfo;
    layoutinfo.bindings.reserve(info->bindingCount);
    bool isSorted = true;
    int lastBinding = -1;
    for (int i = 0; i < (int)info->bindingCount; i++)
    {
        layoutinfo.bindings.push_back(info->pBindings[i]);

        // check that the bindings are in strict increasing order
        if ((int)info->pBindings[i].binding > lastBinding)
        {
            lastBinding = info->pBindings[i].binding;
        }
        else
        {
            isSorted = false;
        }
    }
    if (!isSorted)
    {
        std::sort(layoutinfo.bindings.begin(), layoutinfo.bindings.end(),
                  [](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b)
                  { return a.binding < b.binding; });
    }

    auto it = m_layout_cache.find(layoutinfo);
    if (it != m_layout_cache.end())
    {
        return (*it).second;
    }
    else
    {
        VkDescriptorSetLayout layout;
        vkCreateDescriptorSetLayout(device, info, nullptr, &layout);

        // layoutCache.emplace()
        // add to cache
        m_layout_cache[layoutinfo] = layout;
        return layout;
    }
}

void
descriptor_layout_cache::cleanup()
{
    // delete every descriptor layout held
    for (auto pair : m_layout_cache)
    {
        vkDestroyDescriptorSetLayout(device, pair.second, nullptr);
    }
}

vk_utils::descriptor_builder
descriptor_builder::begin(descriptor_layout_cache* layoutCache, descriptor_allocator* allocator)
{
    descriptor_builder builder;

    builder.cache = layoutCache;
    builder.alloc = allocator;
    return builder;
}

vk_utils::descriptor_builder&
descriptor_builder::bind_buffer(uint32_t binding,
                                VkDescriptorBufferInfo* bufferInfo,
                                VkDescriptorType type,
                                VkShaderStageFlags stageFlags)
{
    VkDescriptorSetLayoutBinding newBinding{};

    newBinding.descriptorCount = 1;
    newBinding.descriptorType = type;
    newBinding.pImmutableSamplers = nullptr;
    newBinding.stageFlags = stageFlags;
    newBinding.binding = binding;

    bindings.push_back(newBinding);

    VkWriteDescriptorSet newWrite{};
    newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    newWrite.pNext = nullptr;

    newWrite.descriptorCount = 1;
    newWrite.descriptorType = type;
    newWrite.pBufferInfo = bufferInfo;
    newWrite.dstBinding = binding;

    writes.push_back(newWrite);
    return *this;
}

vk_utils::descriptor_builder&
descriptor_builder::bind_image(uint32_t binding,
                               VkDescriptorImageInfo* imageInfo,
                               VkDescriptorType type,
                               VkShaderStageFlags stageFlags)
{
    VkDescriptorSetLayoutBinding newBinding{};

    newBinding.descriptorCount = 1;
    newBinding.descriptorType = type;
    newBinding.pImmutableSamplers = nullptr;
    newBinding.stageFlags = stageFlags;
    newBinding.binding = binding;

    bindings.push_back(newBinding);

    VkWriteDescriptorSet newWrite{};
    newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    newWrite.pNext = nullptr;

    newWrite.descriptorCount = 1;
    newWrite.descriptorType = type;
    newWrite.pImageInfo = imageInfo;
    newWrite.dstBinding = binding;

    writes.push_back(newWrite);
    return *this;
}

bool
descriptor_builder::build(VkDescriptorSet& set, VkDescriptorSetLayout& layout)
{
    // build layout first
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;

    layoutInfo.pBindings = bindings.data();
    layoutInfo.bindingCount = (uint32_t)bindings.size();

    layout = cache->create_descriptor_layout(&layoutInfo);

    // allocate descriptor
    bool success = alloc->allocate(&set, layout);
    if (!success)
    {
        return false;
    };

    // write descriptor

    for (VkWriteDescriptorSet& w : writes)
    {
        w.dstSet = set;
    }

    vkUpdateDescriptorSets(alloc->device, (uint32_t)writes.size(), writes.data(), 0, nullptr);

    return true;
}

bool
descriptor_builder::build(VkDescriptorSet& set)
{
    VkDescriptorSetLayout layout;
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
    using std::hash;
    using std::size_t;

    size_t result = hash<size_t>()(bindings.size());

    for (const VkDescriptorSetLayoutBinding& b : bindings)
    {
        // pack the binding data into a single int64. Not fully correct but its ok
        size_t binding_hash =
            b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;

        // shuffle the packed binding data and xor it with the main hash
        result ^= hash<size_t>()(binding_hash);
    }

    return result;
}

}  // namespace vk_utils

}  // namespace agea
