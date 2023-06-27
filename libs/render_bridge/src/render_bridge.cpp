#include "render_bridge/render_bridge.h"

#include <root/smart_object.h>
#include <root/assets/shader_effect.h>
#include <root/root_types_ids.ar.h>

#include <vulkan_render/utils/vulkan_initializers.h>
#include <vulkan_render/types/vulkan_mesh_data.h>
#include <vulkan_render/types/vulkan_texture_data.h>
#include <vulkan_render/types/vulkan_material_data.h>
#include <vulkan_render/types/vulkan_gpu_types.h>
#include <vulkan_render/types/vulkan_shader_effect_data.h>
#include <vulkan_render/types/vulkan_shader_data.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/vulkan_render.h>

#include <utils/agea_log.h>
#include <utils/dynamic_object_builder.h>

namespace agea
{

glob::render_bridge::type glob::render_bridge::s_instance;

namespace
{

const render::vertex_input_description DEFAULT_VERTEX_DESCRIPTION = []()
{
    agea::utils::dynamic_object_layout_sequence_builder<render::gpu_type> builder;
    builder.add_field(AID("pos"), render::gpu_type::g_vec3, 1);
    builder.add_field(AID("norm"), render::gpu_type::g_vec3, 1);
    builder.add_field(AID("color"), render::gpu_type::g_vec3, 1);
    builder.add_field(AID("uv"), render::gpu_type::g_vec2, 1);

    auto dol = builder.get_layout();

    return render::convert_to_vertex_input_description(*dol);
}();

}  // namespace

utils::dynamic_object
render_bridge::extract_gpu_data(root::smart_object& so, const access_template& ct)
{
    auto oitr = ct.offset_in_object.begin();
    auto fitr = ct.layout->get_fields().begin();
    auto src_obj_ptr = so.as_blob();

    utils::dynamic_object dyn_obj(ct.layout);

    AGEA_check(ct.offset_in_object.size() == ct.layout->get_fields().size(), "Should be same!");

    for (; oitr < ct.offset_in_object.end(); ++oitr, ++fitr)
    {
        memcpy(dyn_obj.data() + fitr->offset, src_obj_ptr + *oitr, fitr->size);
    }

    return dyn_obj;
}

bool
render_bridge::create_collection_template(root::smart_object& so, access_template& t)
{
    auto& properties = so.get_reflection()->m_properties;

    t.offset_in_object.clear();

    size_t dst_offest = 0;

    utils::dynamic_object_layout_sequence_builder<render::gpu_type> sb;

    for (auto& p : properties)
    {
        if (!p->gpu_data.empty())
        {
            switch (p->rtype->type_id)
            {
            case root::root__float:
                sb.add_field(AID(""), render::gpu_type::g_float, 1);
                break;
            case root::root__vec3:
                sb.add_field(AID(""), render::gpu_type::g_vec3, 16);
                break;
            case root::root__vec4:
                sb.add_field(AID(""), render::gpu_type::g_vec4, 16);
                break;
            default:
                break;
            }

            t.offset_in_object.push_back((uint32_t)p->offset);
        }
    }

    t.layout = sb.get_layout();

    return true;
}

utils::dynamic_object
render_bridge::collect_gpu_data(root::smart_object& so)
{
    auto itr = m_gpu_data_collection_templates.find(so.get_type_id());
    if (itr == m_gpu_data_collection_templates.end())
    {
        access_template ct;
        create_collection_template(so, ct);

        itr = m_gpu_data_collection_templates.insert({so.get_type_id(), std::move(ct)}).first;
    }

    return extract_gpu_data(so, itr->second);
}

render::shader_effect_create_info
render_bridge::make_se_ci(root::shader_effect& se_model)
{
    render::shader_effect_create_info se_ci;
    se_ci.vert_buffer = &se_model.m_vert;
    se_ci.frag_buffer = &se_model.m_frag;
    se_ci.is_vert_binary = se_model.m_is_vert_binary;
    se_ci.is_frag_binary = se_model.m_is_frag_binary;
    se_ci.is_wire = se_model.m_wire_topology;
    se_ci.enable_alpha = se_model.m_enable_alpha_support;
    se_ci.render_pass = glob::render_device::getr().render_pass();
    se_ci.enable_dynamic_state = false;
    se_ci.vert_input_description = &DEFAULT_VERTEX_DESCRIPTION;

    se_ci.cull_mode = se_ci.is_wire ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

    return se_ci;
}

std::string
render_bridge::make_qid(render::material_data& mt_data, render::mesh_data& m_data)
{
    if (mt_data.get_shader_effect()->m_enable_alpha)
    {
        return "transparent";
    }

    return mt_data.get_id().str() + "::" + m_data.get_id().str();
}

bool
render_bridge::is_agea_texture(const utils::path& p)
{
    return p.has_extention(".atbc");
}

bool
render_bridge::is_agea_mesh(const utils::path& p)
{
    return p.has_extention(".avrt") || p.has_extention(".aind");
}

agea::result_code
render_bridge::render_ctor(root::smart_object& obj, bool sub_objects)
{
    AGEA_check(!obj.has_flag(root::smart_object_state_flag::proto_obj), "");

    if (obj.get_state() == root::smart_object_state::render_ready)
    {
        return result_code::ok;
    }

    AGEA_check(obj.get_state() == root::smart_object_state::constructed, "Shoud not happen");

    obj.set_state(root::smart_object_state::render_preparing);

    get_dependency().build_node(&obj);

    result_code rc = obj.get_reflection()->render_loader(*this, obj, sub_objects);

    obj.set_state(root::smart_object_state::render_ready);

    return rc;
}

agea::result_code
render_bridge::render_dtor(root::smart_object& obj, bool sub_objects)
{
    AGEA_check(!obj.has_flag(root::smart_object_state_flag::proto_obj), "");

    if (obj.get_state() == root::smart_object_state::constructed)
    {
        return result_code::ok;
    }

    AGEA_check(obj.get_state() == root::smart_object_state::render_ready, "Shoud not happen");

    obj.set_state(root::smart_object_state::render_preparing);

    result_code rc = obj.get_reflection()->render_destructor(*this, obj, sub_objects);

    obj.set_state(root::smart_object_state::constructed);

    return rc;
}

}  // namespace agea