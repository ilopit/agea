#pragma once

#include "vulkan_render/types/vulkan_generic.h"

#include "vulkan_render/types/vulkan_gpu_types.h"

#include <utils/id.h>
#include <utils/dynamic_object.h>

namespace agea
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
    material_data(const ::agea::utils::id& id, const ::agea::utils::id& type_id);
    ~material_data();

    const agea::utils::id&
    get_id() const
    {
        return m_id;
    }

    const agea::utils::id&
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
    set_gpu_data(const agea::utils::dynobj& gpu_data)
    {
        m_gpu_data = gpu_data;
    }

    const agea::utils::dynobj&
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

private:
    ::agea::utils::id m_id;
    ::agea::utils::id m_type_id;
    gpu_data_index_type m_type_index = INVALID_GPU_MATERIAL_DATA_INDEX;
    gpu_data_index_type m_index = INVALID_GPU_MATERIAL_DATA_INDEX;

    agea::utils::dynobj m_gpu_data;

    VkDescriptorSet m_samplers_set = VK_NULL_HANDLE;
    shader_effect_data* m_effect = nullptr;
    std::vector<texture_sampler_data> m_texture_samples;
};
}  // namespace render
}  // namespace agea
