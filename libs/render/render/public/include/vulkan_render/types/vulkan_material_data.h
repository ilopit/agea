#pragma once

#include "vulkan_render/types/vulkan_generic.h"

#include "vulkan_render/types/vulkan_gpu_types.h"

#include <utils/id.h>
#include <utils/dynamic_object.h>

namespace kryga
{
namespace render
{
class shader_effect_data;
class texture_data;

struct texture_sampler_data
{
    texture_data* texture = nullptr;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    uint32_t slot = 0;
};

class material_data
{
public:
    material_data(const ::kryga::utils::id& id, const ::kryga::utils::id& type_id);
    ~material_data();

    const kryga::utils::id&
    get_id() const
    {
        return m_id;
    }

    const kryga::utils::id&
    get_type_id() const
    {
        return m_type_id;
    }

    gpu_data_index_type
    gpu_idx() const
    {
        return m_index;
    }

    gpu_data_index_type
    gpu_type_idx() const
    {
        return m_type_index;
    }

    void
    set_indexes(gpu_data_index_type index, gpu_data_index_type mat_type_index)
    {
        m_index = index;
        m_type_index = mat_type_index;
    }

    void
    invalidate_gpu_indexes()
    {
        m_type_index = INVALID_GPU_MATERIAL_DATA_INDEX;
        m_index = INVALID_GPU_MATERIAL_DATA_INDEX;
    }

    bool
    has_textures();

    void
    set_gpu_data(const kryga::utils::dynobj& gpu_data)
    {
        m_gpu_data = gpu_data;
    }

    const kryga::utils::dynobj&
    get_gpu_data() const
    {
        return m_gpu_data;
    }

    const VkDescriptorSet&
    get_textures_ds() const
    {
        return m_samplers_set;
    }

    void
    set_textures_ds(VkDescriptorSet v)
    {
        m_samplers_set = v;
    }

    shader_effect_data*
    get_shader_effect();

    void
    set_shader_effect(shader_effect_data* v)
    {
        m_effect = v;
    }

    void
    set_texture_samples(const std::vector<texture_sampler_data>& v)
    {
        m_texture_samples = v;
    }

    const std::vector<texture_sampler_data>&
    get_texture_samples() const
    {
        return m_texture_samples;
    }

    bool
    has_gpu_data() const
    {
        return !m_gpu_data.empty();
    }

    // Bindless texture indices (indexed by slot)
    void
    set_bindless_texture_index(uint32_t slot, uint32_t bindless_index)
    {
        if (slot >= m_bindless_texture_indices.size())
        {
            m_bindless_texture_indices.resize(slot + 1, UINT32_MAX);
        }
        m_bindless_texture_indices[slot] = bindless_index;
    }

    uint32_t
    get_bindless_texture_index(uint32_t slot) const
    {
        if (slot >= m_bindless_texture_indices.size())
        {
            return UINT32_MAX;
        }
        return m_bindless_texture_indices[slot];
    }

    const std::vector<uint32_t>&
    get_bindless_texture_indices() const
    {
        return m_bindless_texture_indices;
    }

    // Bindless sampler indices (indexed by slot, parallel to texture indices)
    void
    set_bindless_sampler_index(uint32_t slot, uint8_t sampler_index)
    {
        if (slot >= m_bindless_sampler_indices.size())
        {
            m_bindless_sampler_indices.resize(slot + 1, 0);  // Default to LINEAR_REPEAT
        }
        m_bindless_sampler_indices[slot] = sampler_index;
    }

    uint8_t
    get_bindless_sampler_index(uint32_t slot) const
    {
        if (slot >= m_bindless_sampler_indices.size())
        {
            return 0;  // Default to LINEAR_REPEAT
        }
        return m_bindless_sampler_indices[slot];
    }

    const std::vector<uint8_t>&
    get_bindless_sampler_indices() const
    {
        return m_bindless_sampler_indices;
    }

private:
    ::kryga::utils::id m_id;
    ::kryga::utils::id m_type_id;
    gpu_data_index_type m_type_index = INVALID_GPU_MATERIAL_DATA_INDEX;
    gpu_data_index_type m_index = INVALID_GPU_MATERIAL_DATA_INDEX;

    kryga::utils::dynobj m_gpu_data;

    VkDescriptorSet m_samplers_set = VK_NULL_HANDLE;
    shader_effect_data* m_effect = nullptr;
    std::vector<texture_sampler_data> m_texture_samples;
    std::vector<uint32_t> m_bindless_texture_indices;
    std::vector<uint8_t> m_bindless_sampler_indices;  // Parallel to texture indices
};
}  // namespace render
}  // namespace kryga
