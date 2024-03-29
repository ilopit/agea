#include "vulkan_render/types/vulkan_material_data.h"

#include <utils/id.h>
#include <utils/dynamic_object.h>

#include <vulkan_render/types/vulkan_shader_effect_data.h>

namespace agea
{
namespace render
{

material_data::material_data(const ::agea::utils::id& id, const ::agea::utils::id& type_id)
    : m_id(id)
    , m_type_id(type_id)
{
}

material_data::~material_data()
{
}

bool
material_data::has_textures()
{
    return m_samplers_set != VK_NULL_HANDLE;
}

shader_effect_data*
material_data::get_shader_effect()
{
    return m_effect;
}

}  // namespace render
}  // namespace agea