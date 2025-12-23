#include "packages/root/package.root.h"

#include <render_bridge/render_bridge.h>

#include "packages/root/model/assets/mesh.h"
#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture.h"
#include "packages/root/model/assets/shader_effect.h"
#include <glue/type_ids.ar.h>

#include "packages/root/model/game_object.h"

#include <core/reflection/reflection_type.h>
#include <core/reflection/property_utils.h>
#include <core/caches/cache_set.h>
#include <core/object_load_context.h>
#include <core/object_constructor.h>
#include <core/package.h>
#include <core/global_state.h>
#include <core/reflection/reflection_type_utils.h>

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
#include <vulkan_render/agea_render.h>

#include <utils/agea_log.h>
#include <utils/string_utility.h>
#include <utils/dynamic_object_builder.h>

namespace agea
{

namespace root
{

result_code
mesh__render_loader(reflection::type_render_context& ctx)
{
    auto& msh_model = ctx.obj->asr<root::mesh>();

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
mesh__render_destructor(reflection::type_render_context& ctx)
{
    auto& msh_model = ctx.obj->asr<root::mesh>();

    if (auto msh_data = msh_model.get_mesh_data())
    {
        glob::vulkan_render_loader::getr().destroy_mesh_data(msh_data->get_id());
    }

    return result_code::ok;
}

/*===============================*/

result_code
material__render_loader(reflection::type_render_context& ctx)
{
    auto& mat_model = ctx.obj->asr<root::material>();

    auto& txt_models = mat_model.get_texture_samples();

    std::vector<render::texture_sampler_data> samples_data;
    for (auto& ts : txt_models)
    {
        if (ctx.rb->render_ctor(*ts.second.txt, ctx.flag) != result_code::ok)
        {
            return result_code::failed;
        }
        samples_data.emplace_back();
        samples_data.back().texture = ts.second.txt->get_texture_data();
    }

    auto se_model = mat_model.get_shader_effect();

    auto rc = ctx.rb->render_ctor(*se_model, ctx.flag);

    auto se_data = se_model->get_shader_effect_data();
    if (rc != result_code::ok || se_data->m_failed_load)
    {
        se_data = glob::vulkan_render_loader::getr().get_shader_effect_data(AID("se_error"));
    }

    AGEA_check(se_data, "Should exist");

    auto mat_data = glob::vulkan_render_loader::get()->get_material_data(mat_model.get_id());

    auto dyn_gpu_data = ctx.rb->collect_gpu_data(mat_model);

    if (!mat_data)
    {
        mat_data = glob::vulkan_render_loader::get()->create_material(
            mat_model.get_id(), mat_model.get_type_id(), samples_data, *se_data, dyn_gpu_data);

        mat_model.set_material_data(mat_data);
    }
    else
    {
        glob::vulkan_render_loader::get()->update_material(*mat_data, samples_data, *se_data,
                                                           dyn_gpu_data);
    }

    if (!dyn_gpu_data.empty())
    {
        glob::vulkan_render::getr().add_material(mat_data);
        glob::vulkan_render::getr().schedule_material_data_gpu_upload(mat_data);
    }

    return result_code::ok;
}
result_code
material__render_destructor(reflection::type_render_context& ctx)
{
    auto& mat_model = ctx.obj->asr<root::material>();

    if (auto mat_data = mat_model.get_material_data())
    {
        glob::vulkan_render::getr().drop_material(mat_data);
        glob::vulkan_render_loader::getr().destroy_material_data(mat_data->get_id());
    }

    return result_code::ok;
}

/*===============================*/

result_code
texture__render_loader(reflection::type_render_context& ctx)
{
    auto& t = ctx.obj->asr<root::texture>();

    auto& bc = t.get_mutable_base_color();
    auto w = t.get_width();
    auto h = t.get_height();

    render::texture_data* txt_data = nullptr;

    if (::agea::render_bridge::is_agea_texture(bc.get_file()))
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
texture__render_destructor(reflection::type_render_context& ctx)
{
    auto& txt_model = ctx.obj->asr<root::texture>();

    if (auto txt_data = txt_model.get_texture_data())
    {
        glob::vulkan_render_loader::getr().destroy_texture_data(txt_data->get_id());
    }

    return result_code::ok;
}

/*===============================*/

result_code
game_object_component__render_loader(reflection::type_render_context& ctx)
{
    auto& t = ctx.obj->asr<root::game_object_component>();

    auto r = t.get_owner()->asr<root::game_object>().get_components(t.get_order_idx() + 1);

    for (auto& o : r)
    {
        auto rc = ctx.rb->render_ctor(o, false);
        AGEA_return_nok(rc);
    }

    return result_code::ok;
}
result_code
game_object_component__render_destructor(reflection::type_render_context& ctx)
{
    auto& t = ctx.obj->asr<root::game_object_component>();

    auto r = t.get_owner()->asr<root::game_object>().get_components(t.get_order_idx() + 1);

    for (auto& o : r)
    {
        auto rc = ctx.rb->render_dtor(o, false);
        AGEA_return_nok(rc);
    }

    return result_code::ok;
}

result_code
shader_effect__render_loader(reflection::type_render_context& ctx)
{
    auto& se_model = ctx.obj->asr<root::shader_effect>();

    auto se_data = glob::vulkan_render_loader::get()->get_shader_effect_data(se_model.get_id());

    auto se_ci = ::agea::render_bridge::make_se_ci(se_model);

    if (!se_data)
    {
        auto rc = glob::vulkan_render_loader::get()->create_shader_effect(se_model.get_id(), se_ci,
                                                                          se_data);

        AGEA_check(se_data, "Should never happen!");

        se_model.set_shader_effect_data(se_data);

        return rc;
    }
    else
    {
        auto rc = glob::vulkan_render_loader::get()->update_shader_effect(*se_data, se_ci);
        if (rc != result_code::ok)
        {
            ALOG_LAZY_ERROR;
            return rc;
        }
    }
    return result_code::ok;
}

result_code
shader_effect__render_destructor(reflection::type_render_context& ctx)
{
    auto& se_model = ctx.obj->asr<root::shader_effect>();

    if (auto se_data = se_model.get_shader_effect_data())
    {
        glob::vulkan_render_loader::getr().destroy_shader_effect_data(se_data->get_id());
    }

    return result_code::ok;
}

/*===============================*/

}  // namespace root
}  // namespace agea
