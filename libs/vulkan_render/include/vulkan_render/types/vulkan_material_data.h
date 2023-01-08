#pragma once

#include "vulkan_render/types/vulkan_types.h"
#include "vulkan_render/types/vulkan_gpu_types.h"

#include <utils/id.h>
#include <utils/dynamic_object.h>

namespace agea
{
namespace render
{
class shader_effect_data;

class material_data
{
public:
    material_data(const ::agea::utils::id& id, gpu_data_index_type type_id, gpu_data_index_type idx)
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

    gpu_data_index_type
    type_id() const
    {
        return m_type_id;
    }

    gpu_data_index_type
    gpu_idx() const
    {
        return m_index;
    }

    agea::utils::dynamic_object gpu_data;

    VkDescriptorSet texture_set = VK_NULL_HANDLE;
    shader_effect_data* effect = nullptr;
    VkSampler sampler = VK_NULL_HANDLE;

private:
    ::agea::utils::id m_id;
    gpu_data_index_type m_type_id = INVALID_GPU_MATERIAL_DATA_INDEX;
    gpu_data_index_type m_index = INVALID_GPU_MATERIAL_DATA_INDEX;
};
}  // namespace render
}  // namespace agea
