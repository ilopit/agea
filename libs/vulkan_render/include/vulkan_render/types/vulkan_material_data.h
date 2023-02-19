#pragma once

#include "vulkan_render/types/vulkan_generic.h"

#include <utils/id.h>
#include <utils/dynamic_object.h>

namespace agea
{
namespace render
{
class shader_effect_data;

struct texture_sampler_data
{
    texture_data* texture = nullptr;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    uint32_t slot = 0;
};

class material_data
{
public:
    material_data(const ::agea::utils::id& id,
                  const ::agea::utils::id& type_id,
                  gpu_data_index_type idx)
        : m_id(id)
        , m_type_id(type_id)
        , m_index(idx)
    {
    }

    const agea::utils::id&
    get_id() const
    {
        return m_id;
    }

    const agea::utils::id&
    type_id() const
    {
        return m_type_id;
    }

    gpu_data_index_type
    gpu_idx() const
    {
        return m_index;
    }

    void
    set_idx(gpu_data_index_type i)
    {
        m_type_index = i;
    }

    gpu_data_index_type
    gpu_type_idx() const
    {
        return m_type_index;
    }

    agea::utils::dynamic_object gpu_data;

    std::vector<texture_sampler_data> texture_samples;
    shader_effect_data* effect = nullptr;

private:
    ::agea::utils::id m_id;
    ::agea::utils::id m_type_id;
    gpu_data_index_type m_type_index = INVALID_GPU_MATERIAL_DATA_INDEX;
    gpu_data_index_type m_index = INVALID_GPU_MATERIAL_DATA_INDEX;
};
}  // namespace render
}  // namespace agea
