#include "vulkan_render/types/vulkan_material_data.h"

#include <utils/id.h>
#include <utils/dynamic_object.h>

#include <vulkan_render/types/vulkan_shader_effect_data.h>

namespace agea
{
namespace render
{

material_data::~material_data()
{
}

bool
material_data::has_textures()
{
    return !m_effect->fallback ? (m_set != VK_NULL_HANDLE) : false;
}

agea::render::shader_effect_data*
material_data::get_shader_effect()
{
    return !m_effect->fallback ? m_effect : m_effect->fallback;
}

}  // namespace render
}  // namespace agea