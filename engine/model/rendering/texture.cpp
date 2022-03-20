#include "model/rendering/texture.h"

#include "vulkan_render/render_loader.h"

namespace agea
{
namespace model
{
bool
texture::serialize(json_conteiner& c)
{
    return true;
}

bool
texture::deserialize(json_conteiner& c)
{
    base_class::deserialize(c);

    m_base_color = c["base_color"].asString();

    return true;
}

bool
texture::deserialize_finalize(json_conteiner& c)
{
    return true;
}

bool
texture::prepare_for_rendering()
{
    m_texture = glob::render_loader::get()->load_texture(*this);

    return true;
}

}  // namespace model
}  // namespace agea
