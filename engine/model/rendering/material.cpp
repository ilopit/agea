#include "model/rendering/material.h"

#include "model/rendering/texture.h"

#include "vulkan_render/render_loader.h"

namespace agea
{
namespace model
{
bool
material::prepare_for_rendering()
{
    m_texture->prepare_for_rendering();

    m_material = glob::render_loader::get()->load_material(*this);
    return true;
}

bool
material::construct(this_class::construct_params& p)
{
    return true;
}

bool
material::deserialize(json_conteiner& c)
{
    base_class::deserialize(c);

    m_base_effect = c["base_effect"].asString();
    m_texture_id = c["texture"].asString();

    m_texture = smart_object_serializer::object_deserialize_concrete<texture>(m_texture_id,
                                                                              category::assets);

    return true;
}

bool
material::deserialize_finalize(json_conteiner& c)
{
    return true;
}

}  // namespace model
}  // namespace agea
