﻿#pragma once

#include "model/texture.generated.h"

#include "model/assets/asset.h"

#include <utils/buffer.h>

namespace agea
{
namespace render
{
class texture_data;
}  // namespace render

namespace model
{

AGEA_class();
class texture : public asset
{
    AGEA_gen_meta__texture();

public:
    AGEA_gen_class_meta(texture, asset);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_architype_api(texture);

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
    AGEA_property("category=meta", "serializable=true");
    utils::buffer m_base_color;

    AGEA_property("category=meta", "serializable=true");
    uint32_t m_width;

    AGEA_property("category=meta", "serializable=true");
    uint32_t m_height;

    agea::render::texture_data* m_texture_data = nullptr;
};

}  // namespace model
}  // namespace agea
