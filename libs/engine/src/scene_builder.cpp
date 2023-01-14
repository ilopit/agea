#include "engine/scene_builder.h"

#include "engine/agea_engine.h"

#include <model/assets/mesh.h>
#include <model/assets/material.h>
#include <model/assets/texture.h>
#include <model/assets/shader_effect.h>
#include <model/assets/simple_material.h>
#include <model/assets/pbr_material.h>
#include <model/assets/mesh.h>
#include <model/mesh_object.h>
#include <model/point_light.h>
#include <model/components/light_component.h>
#include <model/components/mesh_component.h>
#include <model/demo/demo_mesh_component.h>

#include <assets_importer/mesh_importer.h>
#include <assets_importer/texture_importer.h>

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

#define ARGS this, std::placeholders::_1, std::placeholders::_2

namespace agea
{
namespace
{
std::string
make_qid(render::material_data& mt_data, render::mesh_data& m_data)
{
    if (mt_data.effect->m_enable_alpha)
    {
        return "transparent";
    }

    return m_data.get_id().str() + "::" + m_data.get_id().str();
}

bool
is_agea_texture(const utils::path& p)
{
    return p.has_extention(".atbc");
}

bool
is_agea_mesh(const utils::path& p)
{
    return p.has_extention(".avrt") || p.has_extention(".aind");
}

}  // namespace

scene_builder::scene_builder()
{
    // clang-format off
        m_pfr_handlers[model::mesh::META_type_id()] = std::bind(&scene_builder::pfr_mesh, ARGS);
        m_pfr_handlers[model::material::META_type_id()] = std::bind(&scene_builder::pfr_material, ARGS);
        m_pfr_handlers[model::simple_material::META_type_id()] = std::bind(&scene_builder::pfr_material, ARGS);
        m_pfr_handlers[model::pbr_material::META_type_id()] = std::bind(&scene_builder::pfr_material, ARGS);

        m_pfr_handlers[model::texture::META_type_id()] = std::bind(&scene_builder::pfr_texture, ARGS);
        m_pfr_handlers[model::game_object_component::META_type_id()] = std::bind(&scene_builder::pfr_game_object_component, ARGS);
        m_pfr_handlers[model::game_object::META_type_id()] = std::bind(&scene_builder::pfr_game_object, ARGS);
        m_pfr_handlers[model::mesh_object::META_type_id()] = std::bind(&scene_builder::pfr_game_object, ARGS);
        m_pfr_handlers[model::mesh_component::META_type_id()] = std::bind(&scene_builder::pfr_mesh_component, ARGS);
        m_pfr_handlers[model::demo_mesh_component::META_type_id()] = std::bind(&scene_builder::pfr_mesh_component, ARGS);

        m_pfr_handlers[model::shader_effect::META_type_id()] = std::bind(&scene_builder::pfr_shader_effect, ARGS);
        m_pfr_handlers[model::light_component::META_type_id()] = std::bind(&scene_builder::pfr_game_object_component, ARGS);
        m_pfr_handlers[model::point_light::META_type_id()] = std::bind(&scene_builder::pfr_game_object, ARGS);


        m_sfr_handlers[model::game_object::META_type_id()] = std::bind(&scene_builder::sfr_game_object, ARGS);
        m_sfr_handlers[model::game_object_component::META_type_id()] = std::bind(&scene_builder::sfr_game_object_component, ARGS);
        m_sfr_handlers[model::mesh_object::META_type_id()] = std::bind(&scene_builder::sfr_game_object, ARGS);
        m_sfr_handlers[model::point_light::META_type_id()] = std::bind(&scene_builder::sfr_game_object, ARGS);
        m_sfr_handlers[model::mesh_component::META_type_id()] = std::bind(&scene_builder::sfr_mesh_component, ARGS);
        m_sfr_handlers[model::light_component::META_type_id()] = std::bind(&scene_builder::sfr_game_object_component, ARGS);
        m_sfr_handlers[model::demo_mesh_component::META_type_id()] = std::bind(&scene_builder::sfr_mesh_component, ARGS);
    // clang-format on
}

utils::dynamic_object
scene_builder::extract_gpu_data(model::smart_object& so,
                                const scene_builder::collection_template& ct)
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
scene_builder::create_collection_template(model::smart_object& so,
                                          scene_builder::collection_template& t)
{
    auto& properties = so.reflection()->m_properties;

    t.offset_in_object.clear();

    size_t dst_offest = 0;

    utils::dynamic_object_layout_sequence_builder sb;

    for (auto& p : properties)
    {
        if (!p->gpu_data.empty())
        {
            switch (p->type.type)
            {
            case utils::agea_type::t_f:
                sb.add_field(AID(p->name), p->type.type, (uint32_t)p->size);
                break;

            case utils::agea_type::t_vec4:
            case utils::agea_type::t_vec3:
            {
                sb.add_field(AID(p->name), p->type.type, (uint32_t)p->size, 16);
                break;
            }
            default:
                AGEA_never("Unsupported type!");
            }
            t.offset_in_object.push_back((uint32_t)p->offset);
        }
    }

    t.layout = sb.get_obj();

    return true;
}

utils::dynamic_object
scene_builder::collect_gpu_data(model::smart_object& so)
{
    auto itr = m_gpu_data_collection_templates.find(so.get_type_id());
    if (itr == m_gpu_data_collection_templates.end())
    {
        collection_template ct;
        create_collection_template(so, ct);

        itr = m_gpu_data_collection_templates.insert({so.get_type_id(), std::move(ct)}).first;
    }

    return extract_gpu_data(so, itr->second);
}

bool
scene_builder::pfr_mesh(model::smart_object& obj, bool sub_object)
{
    auto& msh_model = obj.asr<model::mesh>();

    auto vertices = msh_model.get_vertices_buffer().make_view<render::gpu_vertex_data>();
    auto indices = msh_model.get_indicess_buffer().make_view<render::gpu_index_data>();

    if (msh_model.get_vertices_buffer().get_file().empty() &&
        msh_model.get_indicess_buffer().get_file().empty())
    {
        if (!asset_importer::mesh_importer::extract_mesh_data_from_3do(
                msh_model.get_external_buffer().get_file(), vertices, indices))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    auto md = glob::vulkan_render_loader::get()->create_mesh(msh_model.get_id(), vertices, indices);

    msh_model.set_mesh_data(md);

    return true;
}

bool
scene_builder::pfr_material(model::smart_object& obj, bool sub_object)
{
    auto& mat_model = obj.asr<model::material>();

    auto txt_model = mat_model.get_base_texture();

    if (!prepare_for_rendering(*txt_model, sub_object))
    {
        return false;
    }

    auto se_model = mat_model.get_shader_effect();

    if (!prepare_for_rendering(*se_model, sub_object))
    {
        return false;
    }

    auto txt_data = txt_model->get_texture_data();
    auto se_data = se_model->get_shader_effect_data();

    AGEA_check(txt_data && se_data, "Should exist");

    auto mat_data = glob::vulkan_render_loader::get()->get_material_data(mat_model.get_id());

    auto dyn_gpu_data = collect_gpu_data(mat_model);

    if (!mat_data)
    {
        mat_data = glob::vulkan_render_loader::get()->create_material(
            mat_model.get_id(), mat_model.get_type_id(), *txt_data, *se_data, dyn_gpu_data);

        mat_model.set_material_data(mat_data);
        glob::vulkan_render::getr().update_ssbo_data_ranges(mat_data->type_id());
    }
    else
    {
        glob::vulkan_render_loader::get()->update_material(*mat_data, *txt_data, *se_data,
                                                           dyn_gpu_data);
    }

    glob::vulkan_render::getr().schedule_material_data_gpu_transfer(mat_data);

    return true;
}

bool
scene_builder::pfr_texture(model::smart_object& obj, bool sub_object)
{
    auto& t = obj.asr<model::texture>();

    auto& bc = t.get_mutable_base_color();
    auto w = t.get_width();
    auto h = t.get_height();

    render::texture_data* txt_data = nullptr;

    if (is_agea_texture(bc.get_file()))
    {
        txt_data = glob::vulkan_render_loader::get()->create_texture(t.get_id(), bc, w, h);
    }
    else
    {
        utils::buffer b;
        if (!agea::asset_importer::texture_importer::extract_texture_from_buffer(bc, b, w, h))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
        txt_data = glob::vulkan_render_loader::get()->create_texture(t.get_id(), b, w, h);
    }
    t.set_texture_data(txt_data);

    return true;
}

bool
scene_builder::pfr_game_object_component(model::smart_object& obj, bool sub_object)
{
    auto& t = obj.asr<model::game_object_component>();

    for (auto o : t.get_render_components())
    {
        if (!prepare_for_rendering(*o, sub_object))
        {
            return false;
        }
    }
    return true;
}

bool
scene_builder::pfr_game_object(model::smart_object& obj, bool sub_object)
{
    auto& t = obj.asr<model::game_object>();

    auto root = t.get_root_component();

    if (!prepare_for_rendering(*root, sub_object))
    {
        return false;
    }
    return true;
}

bool
scene_builder::pfr_mesh_component(model::smart_object& obj, bool sub_object)
{
    auto& moc = obj.asr<model::mesh_component>();

    if (!prepare_for_rendering(*moc.get_material(), sub_object))
    {
        return false;
    }

    if (!prepare_for_rendering(*moc.get_mesh(), sub_object))
    {
        return false;
    }

    if (sub_object)
    {
        for (auto o : moc.get_render_components())
        {
            if (!prepare_for_rendering(*o, sub_object))
            {
                return false;
            }
        }
    }

    return true;
}

bool
scene_builder::pfr_shader_effect(model::smart_object& obj, bool sub_object)
{
    auto& se_model = obj.asr<model::shader_effect>();

    auto se_data = glob::vulkan_render_loader::get()->get_shader_effect_data(se_model.get_id());

    static render::vertex_input_description input = []()
    {
        agea::utils::dynamic_object_layout_sequence_builder builder;
        builder.add_field(AID("pos"), agea::utils::agea_type::t_vec3, 1);
        builder.add_field(AID("norm"), agea::utils::agea_type::t_vec3, 1);
        builder.add_field(AID("color"), agea::utils::agea_type::t_vec3, 1);
        builder.add_field(AID("uv"), agea::utils::agea_type::t_vec2, 1);

        auto dol = builder.get_obj();

        return render::convert_to_vertex_input_description(*dol);
    }();

    if (!se_data)
    {
        se_data = glob::vulkan_render_loader::get()->create_shader_effect(
            se_model.get_id(), se_model.m_vert, se_model.m_is_vert_binary, se_model.m_frag,
            se_model.m_is_frag_binary, se_model.m_wire_topology, se_model.m_enable_alpha_support,
            false, glob::render_device::getr().render_pass(), input);

        if (!se_data)
        {
            ALOG_LAZY_ERROR;
            return false;
        }
        se_model.set_shader_effect_data(se_data);
    }
    else
    {
        if (!glob::vulkan_render_loader::get()->update_shader_effect(
                *se_data, se_model.m_vert, se_model.m_is_vert_binary, se_model.m_frag,
                se_model.m_is_frag_binary, se_model.m_wire_topology,
                se_model.m_enable_alpha_support, false, glob::render_device::getr().render_pass(),
                input))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }
    return true;
}

bool
scene_builder::pfr_empty(model::smart_object&, bool)
{
    return true;
}

bool
scene_builder::schedule_for_rendering(model::smart_object& obj, bool sub_objects)
{
    AGEA_check(!obj.has_state(model::smart_object_internal_state::class_obj), "");

    if (obj.get_state() == model::smart_objet_state__render_scheduled)
    {
        return true;
    }

    AGEA_check(obj.get_state() == model::smart_objet_state__render_created, "Shoud not happen");

    auto itr = m_sfr_handlers.find(obj.get_type_id());

    if (itr != m_sfr_handlers.end())
    {
        obj.set_state(model::smart_objet_state__render_scheduling);

        auto result = itr->second(obj, sub_objects);

        obj.set_state(model::smart_objet_state__render_scheduled);

        return result;
    }

    return false;
}

bool
scene_builder::sfr_game_object(model::smart_object& obj, bool sub_object)
{
    auto& t = obj.asr<model::game_object>();

    auto root = t.get_root_component();

    if (!schedule_for_rendering(*root, sub_object))
    {
        return false;
    }

    return true;
}

bool
scene_builder::sfr_game_object_component(model::smart_object& obj, bool sub_object)
{
    auto& t = obj.asr<model::game_object_component>();

    if (sub_object)
    {
        for (auto o : t.get_render_components())
        {
            if (!schedule_for_rendering(*o, sub_object))
            {
                return false;
            }
        }
    }

    return true;
}

bool
scene_builder::sfr_mesh_component(model::smart_object& obj, bool sub_object)
{
    auto& moc = obj.asr<model::mesh_component>();

    auto object_data = moc.get_object_dat();
    auto mat_data = moc.get_material()->get_material_data();
    auto mesh_data = moc.get_mesh()->get_mesh_data();

    moc.update_matrix();

    if (!object_data)
    {
        object_data = glob::vulkan_render_loader::getr().create_object(
            moc.get_id(), *mat_data, *mesh_data, moc.get_transofrm_matrix(),
            moc.get_normal_matrix(), moc.get_position());

        moc.set_object_dat(object_data);

        auto new_rqid = make_qid(*mat_data, *mesh_data);
        object_data->queue_id = new_rqid;
        glob::vulkan_render::getr().add_object(object_data);
    }
    else
    {
        if (!glob::vulkan_render_loader::getr().update_object(
                *object_data, *mat_data, *mesh_data, moc.get_transofrm_matrix(),
                moc.get_normal_matrix(), moc.get_position()))
        {
            ALOG_LAZY_ERROR;
            return false;
        }

        auto new_rqid = make_qid(*mat_data, *mesh_data);
        auto& rqid = object_data->queue_id;
        if (new_rqid != rqid)
        {
            glob::vulkan_render::getr().drop_object(object_data);
            object_data->queue_id = new_rqid;
            glob::vulkan_render::getr().add_object(object_data);
        }
    }

    glob::vulkan_render::getr().schedule_game_data_gpu_transfer(object_data);

    if (sub_object)
    {
        for (auto o : moc.get_render_components())
        {
            if (!schedule_for_rendering(*o, sub_object))
            {
                return false;
            }
        }
    }

    return true;
}

bool
scene_builder::prepare_for_rendering(model::smart_object& obj, bool sub_objects)
{
    AGEA_check(!obj.has_state(model::smart_object_internal_state::class_obj), "");

    if (obj.get_state() == model::smart_objet_state__render_created ||
        obj.get_state() == model::smart_objet_state__render_scheduled)
    {
        return true;
    }

    AGEA_check(obj.get_state() == model::smart_objet_state__constructed, "Shoud not happen");

    auto itr = m_pfr_handlers.find(obj.get_type_id());

    if (itr != m_pfr_handlers.end())
    {
        obj.set_state(model::smart_objet_state__render_creating);

        auto result = itr->second(obj, sub_objects);

        obj.set_state(model::smart_objet_state__render_created);

        return result;
    }

    return false;
}

}  // namespace agea