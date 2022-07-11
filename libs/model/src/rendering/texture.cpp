#include "model/rendering/texture.h"
#include "model/package.h"

#include "vulkan_render/render_loader.h"

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(texture);

bool
texture::prepare_for_rendering()
{
    std::string base_color;
    if (get_base_color().front() != '#')
    {
        base_color = m_package->get_resource_path(get_base_color()).str();
    }
    else
    {
        base_color = get_base_color();
    }

    m_texture = glob::render_loader::get()->load_texture(get_id(), base_color);

    if (!m_texture)
    {
        ALOG_LAZY_ERROR;
    }

    return m_texture;
}

}  // namespace model
}  // namespace agea
