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

KRG_ar_class("architype=shader_effect",
             render_cmd_builder = shader_effect__cmd_builder,
             render_cmd_destroyer = shader_effect__cmd_destroyer);
class shader_effect : public asset
{
    KRG_gen_meta__shader_effect();

public:
    KRG_gen_class_meta(shader_effect, asset);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    void
    mark_render_dirty();

    KRG_ar_property("category=Properties", "serializable=true", "access=no");
    utils::buffer m_vert;

    KRG_ar_property("category=Properties", "serializable=true", "access=no", "default=true");
    bool m_is_vert_binary = false;

    KRG_ar_property("category=Properties", "serializable=true", "access=no");
    utils::buffer m_frag;

    KRG_ar_property("category=Properties", "serializable=true", "access=no", "default=true");
    bool m_is_frag_binary = false;

    KRG_ar_property("category=Properties", "serializable=true", "access=no", "default=true");
    bool m_wire_topology = false;

    KRG_ar_property("category=Properties", "serializable=true", "access=no", "default=true");
    bool m_enable_alpha_support = false;
};

}  // namespace root
}  // namespace kryga
