#include "root/root_module.h"

#include "root/assets/mesh.h"
#include "root/assets/material.h"
#include "root/assets/texture.h"
#include "root/assets/shader_effect.h"
#include "root/assets/pbr_material.h"
#include "root/assets/solid_color_material.h"
#include "root/assets/simple_texture_material.h"
#include "root/assets/mesh.h"
#include "root/mesh_object.h"
#include "root/point_light.h"
#include "root/components/light_component.h"
#include "root/components/mesh_component.h"
#include "root/root_types_ids.ar.h"

#include <model/reflection/reflection_type.h>
#include <model/reflection/property_utils.h>
#include <model/caches/cache_set.h>
#include <model/caches/objects_cache.h>
#include <model/caches/materials_cache.h>
#include <model/object_load_context.h>
#include <model/object_constructor.h>
#include <model/package.h>
#include <model/reflection/reflection_type_utils.h>

#include <utils/agea_log.h>
#include <utils/string_utility.h>

#include <serialization/serialization.h>

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
#include <utils/dynamic_object_builder.h>

#include <render_bridge/render_bridge.h>

namespace agea
{
namespace root
{

namespace
{

// ========  UTILS  ====================================

#define MAKE_POD_TYPE(id, type_name)                                   \
    {                                                                  \
        auto rt = glob::reflection_type_registry::getr().get_type(id); \
        rt->deserialization = default_deserialize<type_name>;          \
        rt->compare = default_compare<type_name>;                      \
        rt->copy = default_copy<type_name>;                            \
        rt->serialization = default_serialize<type_name>;              \
    }

template <typename T>
result_code
default_copy(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    reflection::utils::as_type<T>(to) = reflection::utils::as_type<T>(from);
    return result_code::ok;
}

template <typename T>
result_code
default_serialize(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    reflection::utils::pack_field<T>(ptr, jc);

    return result_code::ok;
}

template <typename T>
result_code
default_deserialize(AGEA_deserialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(occ);
    AGEA_unused(jc);
    reflection::utils::extract_field<T>(ptr, jc);
    return result_code::ok;
}

template <typename T>
result_code
default_compare(AGEA_compare_handler_args)
{
    return reflection::utils::default_compare<T>(from, to);
}

result_code
load_smart_object(blob_ptr ptr,
                  const serialization::conteiner& jc,
                  model::object_load_context& occ,
                  model::architype a_type)
{
    auto& field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);

    const auto id = AID(jc.as<std::string>());

    return model::object_constructor::object_load_internal(id, occ, field);
}

result_code
copy_smart_object(AGEA_copy_handler_args)
{
    auto& s = src_obj;
    auto type = ooc.get_construction_type();
    if (type != model::object_load_type::class_obj)
    {
        auto& obj = reflection::utils::as_type<::agea::root::smart_object*>(from);
        auto& dst_obj = reflection::utils::as_type<::agea::root::smart_object*>(to);
        return model::object_constructor::object_clone_create_internal(obj->get_id(), obj->get_id(),
                                                                       ooc, dst_obj);
    }
    else
    {
        reflection::utils::default_copy<root::smart_object*>(from, to);
    }

    return result_code::ok;
}

// ========  ID  ====================================
result_code
serialize_t_id(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    jc = reflection::utils::as_type<utils::id>(ptr).str();
    return result_code::ok;
}

result_code
deserialize_t_id(AGEA_deserialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(occ);
    AGEA_unused(jc);
    reflection::utils::as_type<utils::id>(ptr) = AID(jc.as<std::string>());
    return result_code::ok;
}

// ========  VEC3  ====================================
result_code
serialize_t_vec3(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto& field = reflection::utils::as_type<root::vec3>(ptr);

    jc["x"] = field.x;
    jc["y"] = field.y;
    jc["z"] = field.z;

    return result_code::ok;
}

result_code
deserialize_t_vec3(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    auto& field = reflection::utils::as_type<root::vec3>(ptr);

    field.x = jc["x"].as<float>();
    field.y = jc["y"].as<float>();
    field.z = jc["z"].as<float>();

    return result_code::ok;
}

// ========  TEXTURE  ====================================
result_code
serialize_t_txt(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::utils::as_type<::agea::root::texture*>(ptr);
    jc = field->get_id().str();

    return result_code::ok;
}

result_code
deserialize_t_txt(AGEA_deserialization_args)
{
    return load_smart_object(ptr, jc, occ, model::architype::texture);
}

// ========  MATERIAL  ====================================
result_code
serialize_t_mat(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::utils::as_type<::agea::root::material*>(ptr);
    jc = field->get_id().str();

    return result_code::ok;
}

result_code
deserialize_t_mat(AGEA_deserialization_args)
{
    return load_smart_object(ptr, jc, occ, model::architype::material);
}

result_code
copy_t_mat(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    reflection::utils::default_copy<root::smart_object*>(from, to);
    return result_code::ok;
}

// ========  MESH  ====================================
result_code
serialize_t_msh(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::utils::as_type<::agea::root::mesh*>(ptr);
    jc = field->get_id().str();

    return result_code::ok;
}

result_code
deserialize_t_msh(AGEA_deserialization_args)
{
    return load_smart_object(ptr, jc, occ, model::architype::mesh);
}

result_code
copy_t_msh(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    reflection::utils::default_copy<root::smart_object*>(from, to);
    return result_code::ok;
}

// ========  OBJ  ====================================
result_code
serialize_t_obj(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);

    jc["id"] = field->get_id().str();

    return result_code::ok;
}

result_code
deserialize_t_obj(AGEA_deserialization_args)
{
    AGEA_unused(occ);

    auto field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);

    auto id = AID(jc["id"].as<std::string>());

    //     auto pstr = ::agea::glob::class_objects_cache::get()->get_item(id);
    //
    //     field = pstr;

    return result_code::ok;
}

result_code
deserialize_from_proto_t_obj(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    auto& field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);

    ::agea::model::object_constructor::update_object_properties(*field, jc, occ);

    return result_code::ok;
}

result_code
copy_t_obj(AGEA_copy_handler_args)
{
    AGEA_unused(dst_obj);
    AGEA_unused(src_obj);
    AGEA_unused(ooc);

    AGEA_unused(from);
    AGEA_unused(to);

    AGEA_never("We should never be here!");
    return result_code::ok;
}

// ========  SHADER_EFFECT  ====================================
result_code
serialize_t_se(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);
    jc = field->get_id().str();

    return result_code::ok;
}

result_code
deserialize_t_se(AGEA_deserialization_args)
{
    return load_smart_object(ptr, jc, occ, model::architype::shader_effect);
}

result_code
copy_t_se(AGEA_copy_handler_args)
{
    return copy_smart_object(src_obj, dst_obj, from, to, ooc);
}  // namespace

// ========  COMPONENT  ====================================
result_code
serialize_t_com(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);

    jc["id"] = field->get_id().str();
    jc["object_class"] = field->get_class_obj()->get_id().str();

    return result_code::ok;
}

result_code
deserialize_t_com(AGEA_deserialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);
    AGEA_unused(occ);

    AGEA_not_implemented;

    return result_code::ok;
}

result_code
deserialize_from_proto_t_com(AGEA_deserialization_update_args)
{
    AGEA_unused(occ);

    auto& field = reflection::utils::as_type<::agea::root::smart_object*>(ptr);

    ::agea::model::object_constructor::update_object_properties(*field, jc, occ);

    return result_code::ok;
}

result_code
copy_t_com(AGEA_copy_handler_args)
{
    AGEA_unused(src_obj);

    auto& f = reflection::utils::as_type<::agea::root::smart_object*>(from);
    auto& t = reflection::utils::as_type<::agea::root::smart_object*>(to);

    auto new_id = AID(dst_obj.get_id().str() + "/" + f->get_class_obj()->get_id().str());

    auto p = model::object_constructor::object_clone_create_internal(*f, new_id, ooc, t);

    t = (::agea::root::component*)p;

    return result_code::ok;
}

// ========  COLOR  ====================================
result_code
serialize_t_color(AGEA_serialization_args)
{
    AGEA_unused(ptr);
    AGEA_unused(jc);

    auto& field = reflection::utils::as_type<::agea::model::color>(ptr);

    return result_code::ok;
}

result_code
deserialize_t_color(AGEA_deserialization_args)
{
    auto str_color = jc.as<std::string>();

    if (str_color.size() != 0)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    uint8_t rgba[4] = {0, 0, 0, 255};

    agea::string_utils::convert_hext_string_to_bytes(8, str_color.data(), rgba);

    auto& field = reflection::utils::as_type<::agea::model::color>(ptr);

    field.m_data.r = rgba[0] ? (rgba[0] / 255.f) : 0.f;
    field.m_data.g = rgba[1] ? (rgba[1] / 255.f) : 0.f;
    field.m_data.b = rgba[2] ? (rgba[2] / 255.f) : 0.f;
    field.m_data.a = rgba[3] ? (rgba[3] / 255.f) : 0.f;

    return result_code::ok;
}

result_code
copy_t_color(AGEA_copy_handler_args)
{
    AGEA_unused(src_obj);

    reflection::utils::default_copy<::agea::model::color>(from, to);
    return result_code::ok;
}

result_code
compare_t_color(AGEA_compare_handler_args)
{
    return reflection::utils::default_compare<::agea::model::color>(from, to);
}

// ========  BUFFER  ====================================
result_code
serialize_t_buf(AGEA_serialization_args)
{
    auto& field = reflection::utils::as_type<::agea::utils::buffer>(ptr);

    auto package_path = obj.get_package()->get_relative_path(field.get_file());

    if (!utils::buffer::save(field))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    jc = package_path.str();

    return result_code::ok;
}

result_code
deserialize_t_buf(AGEA_deserialization_args)
{
    auto rel_path = APATH(jc.as<std::string>());

    utils::path package_path;

    if (!occ.make_full_path(rel_path, package_path))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    auto& f = reflection::utils::as_type<::agea::utils::buffer>(ptr);
    f.set_file(package_path);

    if (!utils::buffer::load(package_path, f))
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    return result_code::ok;
}

result_code
copy_t_buf(AGEA_copy_handler_args)
{
    AGEA_unused(src_obj);

    reflection::utils::default_copy<::agea::utils::buffer>(from, to);

    return result_code::ok;
}

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

render::shader_effect_create_info
make_se_ci(root::shader_effect& se_model)
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
    se_ci.cull_mode = VK_CULL_MODE_BACK_BIT;

    return se_ci;
}

///========

result_code
pfr_mesh(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& msh_model = obj.asr<root::mesh>();

    auto vertices = msh_model.get_vertices_buffer().make_view<render::gpu_vertex_data>();
    auto indices = msh_model.get_indicess_buffer().make_view<render::gpu_index_data>();

    if (!msh_model.get_vertices_buffer().size())
    {
        if (!asset_importer::mesh_importer::extract_mesh_data_from_3do(
                msh_model.get_external_buffer().get_file(), vertices, indices))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }
    }

    auto md = glob::vulkan_render_loader::get()->create_mesh(msh_model.get_id(), vertices, indices);

    msh_model.set_mesh_data(md);

    return result_code::ok;
}

result_code
pfr_material(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& mat_model = obj.asr<root::material>();

    auto txt_models = mat_model.get_texture_samples();

    std::vector<render::texture_sampler_data> samples_data;
    for (auto& ts : txt_models)
    {
        if (rb.prepare_for_rendering(*ts.second.txt, sub_object) != result_code::ok)
        {
            return result_code::failed;
        }
        samples_data.emplace_back();
        samples_data.back().texture = ts.second.txt->get_texture_data();
    }

    auto se_model = mat_model.get_shader_effect();
    if (rb.prepare_for_rendering(*se_model, sub_object) != result_code::ok)
    {
        return result_code::failed;
    }

    auto se_data = se_model->get_shader_effect_data();

    AGEA_check(se_data, "Should exist");

    auto mat_data = glob::vulkan_render_loader::get()->get_material_data(mat_model.get_id());

    auto dyn_gpu_data = rb.collect_gpu_data(mat_model);

    if (!mat_data)
    {
        mat_data = glob::vulkan_render_loader::get()->create_material(
            mat_model.get_id(), mat_model.get_type_id(), samples_data, *se_data, dyn_gpu_data);

        mat_model.set_material_data(mat_data);

        if (mat_data->gpu_data.size())
        {
            auto type_inx = glob::vulkan_render::getr().generate_material_ssbo_data_range(
                mat_data->type_id(), mat_data->gpu_data.size());

            mat_data->set_idx(type_inx);
        }
    }
    else
    {
        glob::vulkan_render_loader::get()->update_material(*mat_data, samples_data, *se_data,
                                                           dyn_gpu_data);
    }

    glob::vulkan_render::getr().schedule_material_data_gpu_transfer(mat_data);

    return result_code::ok;
}

result_code
pfr_texture(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& t = obj.asr<root::texture>();

    auto& bc = t.get_mutable_base_color();
    auto w = t.get_width();
    auto h = t.get_height();

    render::texture_data* txt_data = nullptr;

    if (render_bridge::is_agea_texture(bc.get_file()))
    {
        txt_data = glob::vulkan_render_loader::get()->create_texture(t.get_id(), bc, w, h);
    }
    else
    {
        utils::buffer b;
        if (!agea::asset_importer::texture_importer::extract_texture_from_buffer(bc, b, w, h))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }
        txt_data = glob::vulkan_render_loader::get()->create_texture(t.get_id(), b, w, h);
    }
    t.set_texture_data(txt_data);

    return result_code::ok;
}

result_code
pfr_game_object_component(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    return result_code::ok;
}

result_code
pfr_game_object(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& t = obj.asr<root::game_object>();

    for (auto& o : t.get_renderable_components())
    {
        if (rb.prepare_for_rendering(*o, sub_object) != result_code::ok)
        {
            return result_code::failed;
        }
    }

    return result_code::ok;
}

result_code
pfr_mesh_component(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& moc = obj.asr<root::mesh_component>();

    if (rb.prepare_for_rendering(*moc.get_material(), sub_object) != result_code::ok)
    {
        return result_code::failed;
    }

    if (rb.prepare_for_rendering(*moc.get_mesh(), sub_object) != result_code::ok)
    {
        return result_code::failed;
    }

    auto object_data = moc.get_render_object_data();
    auto mat_data = moc.get_material()->get_material_data();
    auto mesh_data = moc.get_mesh()->get_mesh_data();

    moc.update_matrix();

    if (!object_data)
    {
        object_data = glob::vulkan_render_loader::getr().create_object(
            moc.get_id(), *mat_data, *mesh_data, moc.get_transofrm_matrix(),
            moc.get_normal_matrix(), moc.get_position());

        moc.set_render_object_data(object_data);

        auto new_rqid = render_bridge::make_qid(*mat_data, *mesh_data);
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
            return result_code::failed;
        }

        auto new_rqid = render_bridge::make_qid(*mat_data, *mesh_data);
        auto& rqid = object_data->queue_id;
        if (new_rqid != rqid)
        {
            glob::vulkan_render::getr().drop_object(object_data);
            object_data->queue_id = new_rqid;
            glob::vulkan_render::getr().add_object(object_data);
        }
    }

    glob::vulkan_render::getr().schedule_game_data_gpu_transfer(object_data);

    return pfr_game_object_component(rb, obj, sub_object);
}

result_code
pfr_shader_effect(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& se_model = obj.asr<root::shader_effect>();

    auto se_data = glob::vulkan_render_loader::get()->get_shader_effect_data(se_model.get_id());

    auto se_ci = make_se_ci(se_model);

    if (!se_data)
    {
        se_data = glob::vulkan_render_loader::get()->create_shader_effect(se_model.get_id(), se_ci);

        if (!se_data)
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }
        se_model.set_shader_effect_data(se_data);
    }
    else
    {
        if (!glob::vulkan_render_loader::get()->update_shader_effect(*se_data, se_ci))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }
    }
    return result_code::ok;
}

result_code
pfr_empty(agea::render_bridge&, root::smart_object&, bool)
{
    return result_code::ok;
}

}  // namespace

bool
root_module::override_reflection_types()
{
    auto model_id = AID("root");

    MAKE_POD_TYPE(agea::root::root__string, std::string);
    MAKE_POD_TYPE(agea::root::root__bool, bool);

    MAKE_POD_TYPE(agea::root::root__int8_t, std::int8_t);
    MAKE_POD_TYPE(agea::root::root__int16_t, std::int16_t);
    MAKE_POD_TYPE(agea::root::root__int32_t, std::int32_t);
    MAKE_POD_TYPE(agea::root::root__int64_t, std::int64_t);

    MAKE_POD_TYPE(agea::root::root__uint8_t, std::uint8_t);
    MAKE_POD_TYPE(agea::root::root__uint16_t, std::uint16_t);
    MAKE_POD_TYPE(agea::root::root__uint32_t, std::uint32_t);
    MAKE_POD_TYPE(agea::root::root__uint64_t, std::uint64_t);

    MAKE_POD_TYPE(agea::root::root__float, float);
    MAKE_POD_TYPE(agea::root::root__double, double);

    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__id);

        rt->deserialization = deserialize_t_id;
        rt->compare = default_compare<utils::id>;
        rt->copy = default_copy<utils::id>;
        rt->serialization = serialize_t_id;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__buffer);

        rt->deserialization = deserialize_t_buf;
        rt->copy = default_copy<utils::buffer>;
        rt->serialization = serialize_t_buf;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__vec3);

        rt->deserialization = deserialize_t_vec3;
        rt->compare = default_compare<glm::vec3>;
        rt->copy = default_copy<glm::vec3>;
        rt->serialization = serialize_t_vec3;
    }

    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__smart_object);

        rt->deserialization = deserialize_t_obj;
        rt->compare = default_compare<root::smart_object*>;
        rt->copy = copy_smart_object;
        rt->serialization = serialize_t_obj;
        rt->deserialization_with_proto = deserialize_from_proto_t_obj;
        rt->render = pfr_empty;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__game_object);
        rt->render = pfr_game_object;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__component);

        rt->deserialization = deserialize_t_com;
        rt->compare = default_compare<root::smart_object*>;
        rt->deserialization_with_proto = deserialize_from_proto_t_com;
        rt->render = pfr_empty;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__game_object_component);
        rt->render = pfr_game_object_component;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__mesh_component);
        rt->render = pfr_mesh_component;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__texture);

        rt->deserialization = deserialize_t_txt;
        rt->serialization = serialize_t_txt;
        rt->render = pfr_texture;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__material);

        rt->deserialization = deserialize_t_mat;
        rt->serialization = serialize_t_mat;
        rt->render = pfr_material;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__mesh);

        rt->deserialization = deserialize_t_msh;
        rt->serialization = serialize_t_msh;
        rt->render = pfr_mesh;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__shader_effect);

        rt->deserialization = deserialize_t_se;
        rt->serialization = serialize_t_se;
        rt->render = pfr_shader_effect;
    }

    return true;
}

}  // namespace root
}  // namespace agea