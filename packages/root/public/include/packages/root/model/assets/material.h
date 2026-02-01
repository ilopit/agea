#pragma once

#include "packages/root/model/material.ar.h"

#include "packages/root/model/assets/asset.h"
#include "packages/root/model/assets/texture_slot.h"

#include <unordered_map>

namespace kryga
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
KRG_ar_class("architype=material",
              render_constructor = material__render_loader,
              render_destructor  = material__render_destructor);
class material : public asset
// clang-format on
{
    KRG_gen_meta__material();

public:
    KRG_gen_class_meta(material, asset);
    KRG_gen_construct_params{};

    KRG_gen_meta_api;

    bool
    construct(this_class::construct_params& p);

    ::kryga::render::material_data*
    get_material_data() const
    {
        return m_material_data;
    }

    void
    set_material_data(::kryga::render::material_data* v)
    {
        m_material_data = v;
    }

    std::unordered_map<utils::id, texture_slot>&
    get_texture_slots()
    {
        return m_texture_slots;
    }

    texture_slot&
    get_slot(const utils::id& slot);

    void
    set_slot(const utils::id& slot, const texture_slot&);

protected:
    KRG_ar_property("category=Properties",
                     "access=cpp_only",
                     "invalidates=render",
                     "serializable=true");
    shader_effect* m_shader_effect = nullptr;

    std::unordered_map<utils::id, texture_slot> m_texture_slots;

    ::kryga::render::material_data* m_material_data = nullptr;
};

}  // namespace root
}  // namespace kryga
