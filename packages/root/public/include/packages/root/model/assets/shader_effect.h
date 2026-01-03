#pragma once
#include "packages/root/model/shader_effect.ar.h"

#include "packages/root/model/assets/asset.h"
#include "packages/root/model/core_types/color.h"

#include <utils/buffer.h>

namespace kryga
{
namespace render
{
class shader_effect_data;
}  // namespace render

namespace root
{
class texture;

KRG_ar_class("architype=shader_effect",
              render_constructor = shader_effect__render_loader,
              render_destructor = shader_effect__render_destructor);
class shader_effect : public asset
{
    KRG_gen_meta__shader_effect();

public:
    KRG_gen_class_meta(shader_effect, asset);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    render::shader_effect_data*
    get_shader_effect_data()
    {
        return m_shader_effect_data;
    }

    void
    set_shader_effect_data(render::shader_effect_data* v)
    {
        m_shader_effect_data = v;
    }

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

private:
    render::shader_effect_data* m_shader_effect_data = nullptr;
};

}  // namespace root
}  // namespace kryga
