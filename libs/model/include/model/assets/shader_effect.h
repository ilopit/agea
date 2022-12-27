#pragma once

#include "model/assets/asset.h"
#include "model/core_types/color.h"

#include "shader_effect.generated.h"

namespace agea
{
namespace render
{
struct shader_effect_data;
}  // namespace render

namespace model
{
class texture;

AGEA_class();
class shader_effect : public smart_object
{
    AGEA_gen_meta__shader_effect();

public:
    AGEA_gen_class_meta(shader_effect, asset);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_architype_api(shader_effect);

    AGEA_property("category=meta", "serializable=true", "access=no");
    utils::buffer m_vert;

    AGEA_property("category=meta", "serializable=true", "access=no");
    bool m_is_vert_binary = false;

    AGEA_property("category=meta", "serializable=true", "access=no");
    utils::buffer m_frag;

    AGEA_property("category=meta", "serializable=true", "access=no");
    bool m_is_frag_binary = false;

    AGEA_property("category=meta", "serializable=true", "access=no");
    bool m_wire_topology = false;

    AGEA_property("category=meta", "serializable=true", "access=no");
    color m_color;

    AGEA_property("category=meta", "serializable=true", "access=no");
    bool m_enable_alpha_support = false;

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

private:
    render::shader_effect_data* m_shader_effect_data = nullptr;
};

}  // namespace model
}  // namespace agea
