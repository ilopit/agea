#pragma once

#include "packages/root/model/material.ar.h"

#include "packages/root/model/assets/asset.h"
#include "packages/root/model/assets/texture_slot.h"

#include <utils/slot_handle.h>

#include <unordered_map>

namespace kryga
{
namespace render
{
class material_data;
}

namespace root
{
class shader_effect;
}

namespace root
{
class texture;

// clang-format off
KRG_ar_class("architype=material",
              render_cmd_builder   = material__cmd_builder,
              render_cmd_destroyer = material__cmd_destroyer);
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

    std::unordered_map<utils::id, texture_slot>&
    get_texture_slots()
    {
        return m_texture_slots;
    }

    texture_slot&
    get_slot(const utils::id& slot);

    void
    set_slot(const utils::id& slot, const texture_slot&);

    utils::slot_handle<render::material_data>
    get_render_handle() const
    {
        return m_render_handle;
    }

    void
    set_render_handle(utils::slot_handle<render::material_data> h)
    {
        m_render_handle = h;
    }

protected:
    KRG_ar_property("category=Properties",
                    "access=cpp_only",
                    "invalidates=render",
                    "serializable=true");
    shader_effect* m_shader_effect = nullptr;

    std::unordered_map<utils::id, texture_slot> m_texture_slots;

    utils::slot_handle<render::material_data> m_render_handle;
};

}  // namespace root
}  // namespace kryga
