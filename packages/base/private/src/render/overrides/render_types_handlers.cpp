#include "packages/base/package.base.h"

#include <render_bridge/render_bridge.h>

#include "packages/root/model/assets/mesh.h"
#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture.h"
#include "packages/root/model/assets/shader_effect.h"
#include "packages/base/model/assets/pbr_material.h"
#include "packages/base/model/assets/solid_color_material.h"
#include "packages/base/model/assets/simple_texture_material.h"
#include "packages/root/render/overrides/render_types_handlers.h"
#include "glue/type_ids.ar.h"

#include "packages/base/model/components/mesh_component.h"
#include "packages/root/model/game_object.h"

#include "packages/base/model/lights/point_light.h"
#include "packages/base/model/lights/spot_light.h"
#include "packages/base/model/lights/directional_light.h"
#include "packages/base/model/lights/components/directional_light_component.h"
#include "packages/base/model/lights/components/spot_light_component.h"
#include "packages/base/model/lights/components/point_light_component.h"

#include "packages/base/model/mesh_object.h"

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

bool
is_same_source(root::smart_object& obj, root::smart_object& sub_obj)
{
    return obj.get_package() == sub_obj.get_package() || obj.get_level() == sub_obj.get_level();
}

/*===============================*/

result_code
mesh_component__render_loader(reflection::type_context__render& ctx)
{
    auto rc = root::game_object_component__render_loader(ctx);
    AGEA_return_nok(rc);

    auto& moc = ctx.obj->asr<base::mesh_component>();

    if (!moc.get_material() || !moc.get_mesh())
    {
        return result_code::ok;
    }

    rc = ctx.rb->render_ctor(*moc.get_material(), ctx.flag);
    AGEA_return_nok(rc);

    rc = ctx.rb->render_ctor(*moc.get_mesh(), ctx.flag);
    AGEA_return_nok(rc);

    auto mat_data = moc.get_material()->get_material_data();
    auto mesh_data = moc.get_mesh()->get_mesh_data();

    if (!mat_data || !mesh_data)
    {
        return result_code::ok;
    }

    auto object_data = moc.get_render_object_data();

    moc.update_matrix();

    // Calculate scaled bounding radius
    auto scale = moc.get_scale();
    float max_scale = glm::max(glm::max(glm::abs(scale.x), glm::abs(scale.y)), glm::abs(scale.z));
    float scaled_radius = mesh_data->m_bounding_radius * max_scale;

    if (!object_data)
    {
        object_data = glob::vulkan_render::getr().get_cache().objects.alloc(moc.get_id());

        if (!glob::vulkan_render_loader::getr().update_object(
                *object_data, *mat_data, *mesh_data, moc.get_transform_matrix(),
                moc.get_normal_matrix(), glm::vec3(moc.get_world_position())))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }

        object_data->bounding_radius = scaled_radius;
        moc.set_render_object_data(object_data);

        auto new_rqid = ::agea::render_bridge::make_qid(*mat_data, *mesh_data);
        object_data->queue_id = new_rqid;
        glob::vulkan_render::getr().schedule_to_drawing(object_data);
    }
    else
    {
        if (!glob::vulkan_render_loader::getr().update_object(
                *object_data, *mat_data, *mesh_data, moc.get_transform_matrix(),
                moc.get_normal_matrix(), glm::vec3(moc.get_world_position())))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }

        object_data->bounding_radius = scaled_radius;

        auto new_rqid = ::agea::render_bridge::make_qid(*mat_data, *mesh_data);
        auto& rqid = object_data->queue_id;
        if (new_rqid != rqid)
        {
            glob::vulkan_render::getr().remove_from_drawing(object_data);
            object_data->queue_id = new_rqid;
            glob::vulkan_render::getr().schedule_to_drawing(object_data);
        }
    }

    glob::vulkan_render::getr().schedule_game_data_gpu_upload(object_data);

    return result_code::ok;
}

result_code
mesh_component__render_destructor(reflection::type_context__render& ctx)
{
    auto& moc = ctx.obj->asr<base::mesh_component>();

    auto mat_model = moc.get_material();

    result_code rc = result_code::nav;
    if (mat_model && is_same_source(*ctx.obj, *mat_model))
    {
        rc = ctx.rb->render_dtor(*moc.get_material(), ctx.flag);
        AGEA_return_nok(rc);
    }

    auto mesh_model = moc.get_mesh();

    if (mesh_model && is_same_source(*ctx.obj, *mesh_model))
    {
        rc = ctx.rb->render_dtor(*moc.get_mesh(), ctx.flag);
        AGEA_return_nok(rc);
    }
    auto object_data = moc.get_render_object_data();

    if (object_data)
    {
        glob::vulkan_render::getr().remove_from_drawing(object_data);
        glob::vulkan_render::getr().get_cache().objects.release(object_data);
    }

    rc = root::game_object_component__render_destructor(ctx);
    AGEA_return_nok(rc);

    return result_code::ok;
}

/*===============================*/

/*===============================*/

result_code
directional_light_component__render_loader(reflection::type_context__render& ctx)
{
    auto& lc_model = ctx.obj->asr<base::directional_light_component>();

    auto rh = lc_model.get_handler();
    if (!rh)
    {
        rh = glob::vulkan_render::getr().get_cache().directional_lights.alloc(lc_model.get_id());

        rh->gpu_data.ambient = lc_model.get_ambient();
        rh->gpu_data.diffuse = lc_model.get_diffuse();
        rh->gpu_data.specular = lc_model.get_specular();
        rh->gpu_data.direction = lc_model.get_direction();

        lc_model.set_handler(rh);
    }

    glob::vulkan_render::getr().schedule_directional_light_data_gpu_upload(rh);

    return result_code::ok;
}
result_code
directional_light_component__render_destructor(reflection::type_context__render& ctx)
{
    auto& plc_model = ctx.obj->asr<base::directional_light_component>();
    if (auto h = plc_model.get_handler())
    {
        glob::vulkan_render::getr().get_cache().directional_lights.release(h);
    }

    return result_code::ok;
}

/*===============================*/

result_code
spot_light_component__render_loader(reflection::type_context__render& ctx)
{
    auto& lc_model = ctx.obj->asr<base::spot_light_component>();

    auto rh = lc_model.get_handler();
    if (!rh)
    {
        rh = glob::vulkan_render::getr().get_cache().universal_lights.alloc(
            lc_model.get_id(), render::light_type::spot);

        rh->gpu_data.position = lc_model.get_world_position();
        rh->gpu_data.ambient = lc_model.get_ambient();
        rh->gpu_data.diffuse = lc_model.get_diffuse();
        rh->gpu_data.specular = lc_model.get_specular();
        rh->gpu_data.constant = lc_model.get_constant();
        rh->gpu_data.linear = lc_model.get_linear();
        rh->gpu_data.quadratic = lc_model.get_quadratic();
        rh->gpu_data.direction = lc_model.get_direction();
        rh->gpu_data.cut_off = glm::cos(glm::radians(lc_model.get_cut_off()));
        rh->gpu_data.outer_cut_off = glm::cos(glm::radians(lc_model.get_outer_cut_off()));

        lc_model.set_handler(rh);
    }

    glob::vulkan_render::getr().schedule_universal_light_data_gpu_upload(rh);

    return result_code::ok;
}

result_code
spot_light_component__render_destructor(reflection::type_context__render& ctx)
{
    auto& slc_model = ctx.obj->asr<base::spot_light_component>();

    if (auto h = slc_model.get_handler())
    {
        glob::vulkan_render::getr().get_cache().universal_lights.release(h);
    }

    return result_code::ok;
}

/*===============================*/

result_code
point_light_component__render_loader(reflection::type_context__render& ctx)
{
    auto& lc_model = ctx.obj->asr<base::point_light_component>();

    auto rh = lc_model.get_handler();
    if (!rh)
    {
        rh = glob::vulkan_render::getr().get_cache().universal_lights.alloc(
            lc_model.get_id(), render::light_type::point);

        rh->gpu_data.position = lc_model.get_world_position();
        rh->gpu_data.ambient = lc_model.get_ambient();
        rh->gpu_data.diffuse = lc_model.get_diffuse();
        rh->gpu_data.specular = lc_model.get_specular();
        rh->gpu_data.constant = lc_model.get_constant();
        rh->gpu_data.linear = lc_model.get_linear();
        rh->gpu_data.quadratic = lc_model.get_quadratic();

        lc_model.set_handler(rh);
    }

    glob::vulkan_render::getr().schedule_universal_light_data_gpu_upload(rh);

    return result_code::ok;
}

result_code
point_light_component__render_destructor(reflection::type_context__render& ctx)
{
    auto& plc_model = ctx.obj->asr<base::point_light_component>();
    if (auto h = plc_model.get_handler())
    {
        glob::vulkan_render::getr().get_cache().universal_lights.release(h);
    }

    return result_code::ok;
}

}  // namespace agea
