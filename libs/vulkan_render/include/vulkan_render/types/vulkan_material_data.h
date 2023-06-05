#pragma once

#include "vulkan_render/types/vulkan_generic.h"

#include <utils/id.h>
#include <utils/dynamic_object.h>
#include "vulkan_render/types/vulkan_gpu_types.h"

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
    material_data(const ::agea::utils::id& id, const ::agea::utils::id& type_id)
        : m_id(id)
        , m_type_id(type_id)
    {
    }

    ~material_data();

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

    gpu_data_index_type
    gpu_type_idx() const
    {
        return m_type_index;
    }

    void
    set_idexes(gpu_data_index_type index, gpu_data_index_type mat_type_index)
    {
        m_index = index;
        m_type_index = mat_type_index;
    }

    void
    invalidate_ids()
    {
        m_type_index = INVALID_GPU_MATERIAL_DATA_INDEX;
        m_index = INVALID_GPU_MATERIAL_DATA_INDEX;
    }

    agea::utils::dynamic_object gpu_data;

    std::vector<texture_sampler_data> texture_samples;
    shader_effect_data* effect = nullptr;
    VkDescriptorSet m_set = VK_NULL_HANDLE;

private:
    ::agea::utils::id m_id;
    ::agea::utils::id m_type_id;
    gpu_data_index_type m_type_index = INVALID_GPU_MATERIAL_DATA_INDEX;
    gpu_data_index_type m_index = INVALID_GPU_MATERIAL_DATA_INDEX;
};
}  // namespace render
}  // namespace agea
