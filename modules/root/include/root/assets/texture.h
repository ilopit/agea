#pragma once

#include "root/texture.generated.h"

#include "root/assets/asset.h"

#include <utils/buffer.h>

namespace agea
{
namespace render
{
class texture_data;
}  // namespace render

namespace root
{

AGEA_ar_class("architype=texture");
class texture : public asset
{
    AGEA_gen_meta__texture();

public:
    AGEA_gen_class_meta(texture, asset);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    utils::buffer&
    get_mutable_base_color()
    {
        return m_base_color;
    }

    render::texture_data*
    get_texture_data() const
    {
        return m_texture_data;
    }

    void
    set_texture_data(render::texture_data* v)
    {
        m_texture_data = v;
    }

protected:
    AGEA_ar_property("category=meta", "serializable=true");
    utils::buffer m_base_color;

    AGEA_ar_property("category=meta", "serializable=true");
    uint32_t m_width;

    AGEA_ar_property("category=meta", "serializable=true");
    uint32_t m_height;

    agea::render::texture_data* m_texture_data = nullptr;
};

}  // namespace root
}  // namespace agea
