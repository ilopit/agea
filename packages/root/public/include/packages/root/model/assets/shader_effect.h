#pragma once
#include "packages/root/model/shader_effect.ar.h"

#include "packages/root/model/assets/asset.h"
#include "packages/root/model/core_types/color.h"

#include <utils/buffer.h>

namespace kryga
{
namespace root
{
class texture;

// clang-format off
KRG_ar_class(
    "architype=shader_effect",
    render_cmd_builder   = shader_effect__cmd_builder,
    render_cmd_destroyer = shader_effect__cmd_destroyer,
    mcp_schema           = "string:asset_id",
    mcp_hint             = "GPU shader program: vertex + fragment — defines the rendering pipeline "
                           "for a material"
);
class shader_effect : public asset
// clang-format on
{
    KRG_gen_meta__shader_effect();

public:
    KRG_gen_class_meta(shader_effect, asset);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    void
    mark_render_dirty();

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = true,
        access       = no
    );
    utils::buffer m_vert;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = true,
        access       = no,
        default      = true
    );
    bool m_is_vert_binary = false;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = true,
        access       = no
    );
    utils::buffer m_frag;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = true,
        access       = no,
        default      = true
    );
    bool m_is_frag_binary = false;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = true,
        access       = no,
        default      = true
    );
    bool m_wire_topology = false;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Properties",
        serializable = true,
        access       = no,
        default      = true
    );
    bool m_enable_alpha_support = false;
    // clang-format on

    // Shader specialization constants — each maps to a constant_id in the shader
    // clang-format off
    KRG_ar_property(
        category     = "Specialization",
        serializable = true,
        access       = no,
        default      = false
    );
    bool m_enable_lightmap = false;
    // clang-format on
};

}  // namespace root
}  // namespace kryga
