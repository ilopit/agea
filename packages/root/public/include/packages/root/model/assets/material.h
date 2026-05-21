#pragma once

#include "packages/root/model/material.ar.h"

#include "packages/root/model/assets/asset.h"

namespace kryga
{
namespace root
{
class shader_effect;

// clang-format off
KRG_ar_class("architype=material",
              render_cmd_builder   = material__cmd_builder,
              render_cmd_destroyer = material__cmd_destroyer,
              mcp_schema           = "string:asset_id",
              mcp_hint             = "Controls surface appearance — references a shader and provides color/texture parameters to it");
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

protected:
    KRG_ar_property("category=Properties",
                    "access=cpp_only",
                    "invalidates=render",
                    "serializable=true",
                    "instantiate=share");
    shader_effect* m_shader_effect = nullptr;
};

}  // namespace root
}  // namespace kryga
