#include "root/render/root_module_render_bridge.h"

#include "root/assets/mesh.h"
#include "root/assets/material.h"
#include "root/assets/texture.h"
#include "root/assets/shader_effect.h"
#include "root/assets/pbr_material.h"
#include "root/assets/solid_color_material.h"
#include "root/assets/simple_texture_material.h"
#include "root/assets/mesh.h"
#include "root/mesh_object.h"
#include "root/lights/point_light.h"
#include "root/lights/spot_light.h"
#include "root/lights/directional_light.h"
#include "root/game_object.h"
#include "root/lights/components/directional_light_component.h"
#include "root/lights/components/spot_light_component.h"
#include "root/lights/components/point_light_component.h"
#include "root/components/mesh_component.h"
#include "root/root_types_ids.ar.h"

#include <core/reflection/reflection_type.h>
#include <core/reflection/property_utils.h>
#include <core/caches/cache_set.h>
#include <core/caches/objects_cache.h>
#include <core/caches/materials_cache.h>
#include <core/object_load_context.h>
#include <core/object_constructor.h>
#include <core/package.h>
#include <core/reflection/reflection_type_utils.h>

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

bool
is_same_source(root::smart_object& obj, root::smart_object& sub_obj)
{
    return obj.get_package() == sub_obj.get_package() || obj.get_level() == sub_obj.get_level();
}

/*===============================*/

result_code
render_ctor__mesh(render_bridge& rb, root::smart_object& obj, bool sub_object)
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
render_dtor__mesh(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& msh_model = obj.asr<root::mesh>();

    if (auto msh_data = msh_model.get_mesh_data())
    {
        glob::vulkan_render_loader::getr().destroy_mesh_data(msh_data->get_id());
    }

    return result_code::ok;
}

/*===============================*/

result_code
render_ctor__material(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& mat_model = obj.asr<root::material>();

    auto txt_models = mat_model.get_texture_samples();

    std::vector<render::texture_sampler_data> samples_data;
    for (auto& ts : txt_models)
    {
        if (rb.render_ctor(*ts.second.txt, sub_object) != result_code::ok)
        {
            return result_code::failed;
        }
        samples_data.emplace_back();
        samples_data.back().texture = ts.second.txt->get_texture_data();
    }

    auto se_model = mat_model.get_shader_effect();
    if (rb.render_ctor(*se_model, sub_object) != result_code::ok)
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

    glob::vulkan_render::getr().schedule_material_data_gpu_upload(mat_data);

    return result_code::ok;
}
result_code
render_dtor__material(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& mat_model = obj.asr<root::material>();

    if (auto mat_data = mat_model.get_material_data())
    {
        glob::vulkan_render_loader::getr().destroy_material_data(mat_data->get_id());
    }

    return result_code::ok;
}

/*===============================*/

result_code
render_ctor__texture(render_bridge& rb, root::smart_object& obj, bool sub_object)
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
render_dtor__texture(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& txt_model = obj.asr<root::texture>();

    if (auto txt_data = txt_model.get_texture_data())
    {
        glob::vulkan_render_loader::getr().destroy_texture_data(txt_data->get_id());
    }

    return result_code::ok;
}

/*===============================*/

result_code
render_ctor__game_object_component(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& t = obj.asr<root::game_object_component>();

    auto r = t.get_owner()->asr<root::game_object>().get_components(t.get_order_idx() + 1);

    for (auto& o : r)
    {
        auto rc = rb.render_ctor(o, false);
        AGEA_return_nok(rc);
    }

    return result_code::ok;
}
result_code
render_dtor__game_object_component(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& t = obj.asr<root::game_object_component>();

    auto r = t.get_owner()->asr<root::game_object>().get_components(t.get_order_idx() + 1);

    for (auto& o : r)
    {
        auto rc = rb.render_dtor(o, false);
        AGEA_return_nok(rc);
    }

    return result_code::ok;
}

/*===============================*/

result_code
render_ctor__mesh_component(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto rc = render_ctor__game_object_component(rb, obj, sub_object);
    AGEA_return_nok(rc);

    auto& moc = obj.asr<root::mesh_component>();

    if (!moc.get_material() || !moc.get_mesh())
    {
        return result_code::ok;
    }

    rc = rb.render_ctor(*moc.get_material(), sub_object);
    AGEA_return_nok(rc);

    rc = rb.render_ctor(*moc.get_mesh(), sub_object);
    AGEA_return_nok(rc);

    auto mat_data = moc.get_material()->get_material_data();
    auto mesh_data = moc.get_mesh()->get_mesh_data();

    if (!mat_data || !mesh_data)
    {
        return result_code::ok;
    }

    auto object_data = moc.get_render_object_data();

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

    glob::vulkan_render::getr().schedule_game_data_gpu_upload(object_data);

    return result_code::ok;
}
result_code
render_dtor__mesh_component(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& moc = obj.asr<root::mesh_component>();

    auto mat_model = moc.get_material();

    result_code rc = result_code::nav;
    if (mat_model && is_same_source(obj, *mat_model))
    {
        rc = rb.render_dtor(*moc.get_material(), sub_object);
        AGEA_return_nok(rc);
    }

    auto mesh_model = moc.get_mesh();

    if (mesh_model && is_same_source(obj, *mesh_model))
    {
        rc = rb.render_dtor(*moc.get_mesh(), sub_object);
        AGEA_return_nok(rc);
    }
    auto object_data = moc.get_render_object_data();

    if (object_data)
    {
        glob::vulkan_render::getr().m_objects_id.release_id(object_data->gpu_index());
        glob::vulkan_render_loader::getr().destroy_object(object_data->get_id());
    }

    rc = render_dtor__game_object_component(rb, obj, sub_object);
    AGEA_return_nok(rc);

    return result_code::ok;
}

/*===============================*/

result_code
render_ctor__shader_effect(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& se_model = obj.asr<root::shader_effect>();

    auto se_data = glob::vulkan_render_loader::get()->get_shader_effect_data(se_model.get_id());

    auto se_ci = render_bridge::make_se_ci(se_model);

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
render_dtor__shader_effect(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& se_model = obj.asr<root::shader_effect>();

    if (auto se_data = se_model.get_shader_effect_data())
    {
        glob::vulkan_render_loader::getr().destroy_shader_effect_data(se_data->get_id());
    }

    return result_code::ok;
}

/*===============================*/

result_code
render_ctor__directional_light_component(render_bridge& rb,
                                         root::smart_object& obj,
                                         bool sub_object)
{
    auto& lc_model = obj.asr<root::directional_light_component>();

    auto rh = lc_model.get_handler();
    if (!rh)
    {
        auto iid = glob::vulkan_render::getr().m_dir_lights_idalloc.alloc_id();

        render::gpu_light ld{};

        ld.directional.ambient = lc_model.get_ambient();
        ld.directional.diffuse = lc_model.get_diffuse();
        ld.directional.specular = lc_model.get_specular();
        ld.directional.direction = lc_model.get_direction();

        rh = glob::vulkan_render_loader::get()->create_light_data(
            lc_model.get_id(), render::light_type::directional_light_data, ld);
        rh->m_gpu_id = iid;

        lc_model.set_handler(rh);
    }

    glob::vulkan_render::getr().schedule_light_data_gpu_upload(rh);

    return result_code::ok;
}

result_code
render_dtor__directional_light_component(render_bridge& rb,
                                         root::smart_object& obj,
                                         bool sub_object)
{
    auto& plc_model = obj.asr<root::directional_light_component>();
    if (auto h = plc_model.get_handler())
    {
        glob::vulkan_render::getr().m_dir_lights_idalloc.release_id(h->m_gpu_id);
        glob::vulkan_render_loader::getr().destroy_light_data(h->get_id());
    }

    return result_code::ok;
}

/*===============================*/

result_code
render_ctor__spot_light_component(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& lc_model = obj.asr<root::spot_light_component>();

    auto rh = lc_model.get_handler();
    if (!rh)
    {
        auto iid = glob::vulkan_render::getr().m_spot_lights_idalloc.alloc_id();

        render::gpu_light ld{};

        ld.spot.position = lc_model.get_world_position();
        ld.spot.ambient = lc_model.get_ambient();
        ld.spot.diffuse = lc_model.get_diffuse();
        ld.spot.specular = lc_model.get_specular();
        ld.spot.constant = lc_model.get_constant();
        ld.spot.linear = lc_model.get_linear();
        ld.spot.quadratic = lc_model.get_quadratic();
        ld.spot.direction = lc_model.get_direction();

        ld.spot.cut_off = glm::cos(glm::radians(lc_model.get_cut_off()));
        ld.spot.outer_cut_off = glm::cos(glm::radians(lc_model.get_outer_cut_off()));

        rh = glob::vulkan_render_loader::get()->create_light_data(
            lc_model.get_id(), render::light_type::spot_light_data, ld);
        rh->m_gpu_id = iid;

        lc_model.set_handler(rh);
    }

    glob::vulkan_render::getr().schedule_light_data_gpu_upload(rh);

    return result_code::ok;
}

result_code
render_dtor__spot_light_component(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& slc_model = obj.asr<root::spot_light_component>();

    if (auto h = slc_model.get_handler())
    {
        glob::vulkan_render::getr().m_spot_lights_idalloc.release_id(h->m_gpu_id);
        glob::vulkan_render_loader::getr().destroy_light_data(h->get_id());
    }

    return result_code::ok;
}

/*===============================*/

result_code
render_ctor__point_light_component(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& lc_model = obj.asr<root::point_light_component>();

    auto rh = lc_model.get_handler();
    if (!rh)
    {
        auto iid = glob::vulkan_render::getr().m_point_lights_idalloc.alloc_id();

        render::gpu_light ld{};

        ld.point.position = lc_model.get_world_position();
        ld.point.ambient = lc_model.get_ambient();
        ld.point.diffuse = lc_model.get_diffuse();
        ld.point.specular = lc_model.get_specular();
        ld.point.constant = lc_model.get_constant();
        ld.point.linear = lc_model.get_linear();
        ld.point.quadratic = lc_model.get_quadratic();

        rh = glob::vulkan_render_loader::get()->create_light_data(
            lc_model.get_id(), render::light_type::point_light_data, ld);
        rh->m_gpu_id = iid;

        lc_model.set_handler(rh);
    }

    glob::vulkan_render::getr().schedule_light_data_gpu_upload(rh);

    return result_code::ok;
}

result_code
render_dtor__point_light_component(render_bridge& rb, root::smart_object& obj, bool sub_object)
{
    auto& plc_model = obj.asr<root::point_light_component>();
    if (auto h = plc_model.get_handler())
    {
        glob::vulkan_render::getr().m_point_lights_idalloc.release_id(h->m_gpu_id);
        glob::vulkan_render_loader::getr().destroy_light_data(h->get_id());
    }

    return result_code::ok;
}
}  // namespace

bool
root_module_render_bridge::override_reflection_types()
{
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__game_object_component);
        rt->render_ctor = render_ctor__game_object_component;
        rt->render_dtor = render_dtor__game_object_component;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__mesh_component);
        rt->render_ctor = render_ctor__mesh_component;
        rt->render_dtor = render_dtor__mesh_component;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__texture);
        rt->render_ctor = render_ctor__texture;
        rt->render_dtor = render_dtor__texture;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__material);
        rt->render_ctor = render_ctor__material;
        rt->render_dtor = render_dtor__material;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__mesh);
        rt->render_ctor = render_ctor__mesh;
        rt->render_dtor = render_dtor__mesh;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__shader_effect);
        rt->render_ctor = render_ctor__shader_effect;
        rt->render_dtor = render_dtor__shader_effect;
    }
    {
        auto rt =
            glob::reflection_type_registry::getr().get_type(root__directional_light_component);
        rt->render_ctor = render_ctor__directional_light_component;
        rt->render_dtor = render_dtor__directional_light_component;
    }
    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__point_light_component);
        rt->render_ctor = render_ctor__point_light_component;
        rt->render_dtor = render_dtor__point_light_component;
    }

    {
        auto rt = glob::reflection_type_registry::getr().get_type(root__spot_light_component);
        rt->render_ctor = render_ctor__spot_light_component;
        rt->render_dtor = render_dtor__spot_light_component;
    }

    return true;
}

root_module_render_bridge&
root_module_render_bridge::instance()
{
    static root_module_render_bridge s_module(AID("root.module.render_bridge"));
    return s_module;
}

}  // namespace root
}  // namespace agea