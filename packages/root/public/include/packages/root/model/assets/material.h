#pragma once

#include "packages/root/model/material.ar.h"

#include "packages/root/model/assets/asset.h"

#include "render_types/render_handle.h"

namespace kryga
{
namespace root
{
class shader_effect;

// clang-format off
KRG_ar_class(
    "architype=material",
    render_cmd_builder   = material__cmd_builder,
    render_cmd_destroyer = material__cmd_destroyer,
    mcp_schema           = "string:asset_id",
    mcp_hint             = "Controls surface appearance — references a shader and provides "
                           "color/texture parameters to it"
);
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

    // Runtime render slot (handle model, slice 3). Reserved by the material cmd
    // builder; draws reference the material by U64 handle. NOT serialized.
    ::kryga::render::types::material_handle
    render_handle() const
    {
        return m_render_handle;
    }
    void
    set_render_handle(::kryga::render::types::material_handle h)
    {
        m_render_handle = h;
    }

protected:
    ::kryga::render::types::material_handle m_render_handle = {};  // runtime, not serialized

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        access       = cpp_only,
        invalidates  = render,
        serializable = true,
        instantiate  = share
    );
    shader_effect* m_shader_effect = nullptr;
    // clang-format on
};

}  // namespace root
}  // namespace kryga
