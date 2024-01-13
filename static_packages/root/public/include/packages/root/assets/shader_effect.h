#pragma once

#include "packages/root/assets/asset.h"
#include "packages/root/core_types/color.h"

#include <utils/buffer.h>

#include "packages/root/shader_effect.generated.h"

namespace agea
{
namespace render
{
class shader_effect_data;
}  // namespace render

namespace root
{
class texture;

AGEA_ar_class("architype=shader_effect");
class shader_effect : public smart_object
{
    AGEA_gen_meta__shader_effect();

public:
    AGEA_gen_class_meta(shader_effect, asset);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

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

    AGEA_ar_property("category=meta", "serializable=true", "access=no");
    utils::buffer m_vert;

    AGEA_ar_property("category=meta", "serializable=true", "access=no", "default=true");
    bool m_is_vert_binary = false;

    AGEA_ar_property("category=meta", "serializable=true", "access=no");
    utils::buffer m_frag;

    AGEA_ar_property("category=meta", "serializable=true", "access=no", "default=true");
    bool m_is_frag_binary = false;

    AGEA_ar_property("category=meta", "serializable=true", "access=no", "default=true");
    bool m_wire_topology = false;

    AGEA_ar_property("category=meta", "serializable=true", "access=no", "default=true");
    bool m_enable_alpha_support = false;

private:
    render::shader_effect_data* m_shader_effect_data = nullptr;
};

}  // namespace root
}  // namespace agea
