#include "model/rendering/texture.h"

#include "vulkan_render/render_loader.h"

namespace agea
{
namespace model
{

AGEA_gen_class_cd_default(texture);

bool
texture::prepare_for_rendering()
{
    m_texture = glob::render_loader::get()->load_texture(*this);

    return true;
}

}  // namespace model
}  // namespace agea
