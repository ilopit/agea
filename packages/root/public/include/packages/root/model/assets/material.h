#pragma once

#include "packages/root/model/material.ar.h"

#include "packages/root/model/assets/asset.h"
#include "packages/root/model/assets/texture_sample.h"

#include <unordered_map>

namespace agea
{
namespace root
{
class shader_effect;
}

namespace render
{
class material_data;
}  // namespace render

namespace root
{
class texture;

// clang-format off
AGEA_ar_class("architype=material",
              render_constructor = material__render_loader,
              render_destructor =  material__render_destructor);
class material : public asset
// clang-format on
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
    AGEA_ar_property("category=Properties",
                     "access=cpp_only",
                     "invalidates=render",
                     "serializable=true");
    shader_effect* m_shader_effect = nullptr;

    std::unordered_map<utils::id, texture_sample> m_texture_samples;

    ::agea::render::material_data* m_material_data = nullptr;
};

}  // namespace root
}  // namespace agea
