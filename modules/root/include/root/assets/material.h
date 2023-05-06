#pragma once

#include "root/material.generated.h"

#include "root/assets/asset.h"
#include "root/assets/texture_sample.h"

namespace agea
{
namespace render
{
class material_data;
}  // namespace render

namespace root
{
class texture;

AGEA_ar_class("architype=material");
class material : public asset
{
    AGEA_gen_meta__material();

public:
    AGEA_gen_class_meta(material, asset);
    AGEA_gen_construct_params{};

    AGEA_gen_meta_api;

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

    std::unordered_map<utils::id, texture_sample>&
    get_texture_samples()
    {
        return m_texture_samples;
    }

    texture_sample&
    get_sample(const utils::id& slot);

    void
    set_sample(const utils::id& slot, const texture_sample&);

protected:
    AGEA_ar_property("category=properties", "access=cpp_only", "serializable=true");
    shader_effect* m_shader_effect = nullptr;

    std::unordered_map<utils::id, texture_sample> m_texture_samples;

    ::agea::render::material_data* m_material_data = nullptr;
};

}  // namespace root
}  // namespace agea
