#pragma once

#include "material.generated.h"

#include "model/assets/asset.h"

namespace agea
{
namespace render
{
class material_data;
}  // namespace render

namespace model
{
class texture;

AGEA_class();
class material : public asset
{
    AGEA_gen_meta__material();

public:
    AGEA_gen_class_meta(material, asset);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_architype_api(material);

    bool
    construct(this_class::construct_params& p);

    ::agea::render::material_data*
    get_material_data() const
    {
        return m_material_data;
    }

    void
    set_material_data(::agea::render::material_data* v)
    {
        m_material_data = v;
    }

    texture*
    get_base_texture() const
    {
        return m_base_texture;
    }

    void
    set_base_texture(texture* base_texture)
    {
        m_base_texture = base_texture;
    }

protected:
    AGEA_property("category=properties", "access=cpp_only", "serializable=true");
    shader_effect* m_shader_effect = nullptr;

    AGEA_property("category=properties", "access=no", "serializable=true");
    texture* m_base_texture = nullptr;

    ::agea::render::material_data* m_material_data = nullptr;
};

}  // namespace model
}  // namespace agea
