#include "packages/base/package.base.h"

#include <global_state/global_state.h>
#include <render_bridge/render_bridge.h>
#include <render_bridge/render_command.h>
#include <render_bridge/render_command_processor.h>
#include <render_bridge/render_commands_common.h>

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
#include "packages/base/model/components/animated_mesh_component.h"
#include "packages/root/model/game_object.h"

#include "packages/base/model/lights/point_light.h"
#include "packages/base/model/lights/spot_light.h"
#include "packages/base/model/lights/directional_light.h"
#include "packages/base/model/lights/components/directional_light_component.h"
#include "packages/base/model/lights/components/spot_light_component.h"
#include "packages/base/model/lights/components/point_light_component.h"

#include "packages/base/model/mesh_object.h"

#include <render/utils/light_grid.h>

#include <core/reflection/reflection_type.h>
#include <core/reflection/property_utils.h>
#include <core/caches/cache_set.h>
#include <core/object_load_context.h>
#include <core/object_constructor.h>
#include <core/package.h>
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
#include <vulkan_render/kryga_render.h>

#include <animation/gltf_animation_loader.h>
#include <animation/ozz_loader.h>
#include <animation/animation_system.h>

#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>

#include <resource_locator/resource_locator.h>

#include <utils/buffer.h>
#include <utils/path.h>
#include <utils/kryga_log.h>
#include <utils/string_utility.h>
#include <utils/dynamic_object_builder.h>

#include <filesystem>

namespace kryga
{

bool
is_same_source(root::smart_object& obj, root::smart_object& sub_obj)
{
    return obj.get_package() == sub_obj.get_package() || obj.get_level() == sub_obj.get_level();
}

// ============================================================================
// Render commands — objects
// ============================================================================

struct create_object_cmd : render_cmd::render_command_base
{
    utils::id id;
    render_cmd::render_handle mesh_handle = render_cmd::k_invalid_handle;
    render_cmd::render_handle material_handle = render_cmd::k_invalid_handle;
    glm::mat4 transform{1.0f};
    glm::mat4 normal_matrix{1.0f};
    glm::vec3 position{0.0f};
    float bounding_radius = 0.0f;
    uint32_t bone_count = 0;
    std::string queue_id;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        auto* mesh_data = ctx.processor.resolve_mesh(mesh_handle);
        auto* mat_data = ctx.processor.resolve_material(material_handle);

        if (!mesh_data || !mat_data)
        {
            ALOG_ERROR("create_object: missing mesh or material for {}", id.str());
            return;
        }

        auto* object_data = ctx.vr.get_cache().objects.alloc(id);

        if (!ctx.loader.update_object(*object_data, *mat_data, *mesh_data, transform,
                                      normal_matrix, position))
        {
            ALOG_LAZY_ERROR;
            return;
        }

        object_data->gpu_data.bounding_radius = bounding_radius;
        object_data->bone_count = bone_count;
        object_data->queue_id = std::move(queue_id);

        ctx.vr.schedule_to_drawing(object_data);
        ctx.vr.schedule_game_data_gpu_upload(object_data);

        auto h = ctx.processor.ensure_handle(id);
        ctx.processor.store_object(h, object_data);
    }
};

struct update_object_cmd : render_cmd::render_command_base
{
    utils::id id;
    render_cmd::render_handle mesh_handle = render_cmd::k_invalid_handle;
    render_cmd::render_handle material_handle = render_cmd::k_invalid_handle;
    glm::mat4 transform{1.0f};
    glm::mat4 normal_matrix{1.0f};
    glm::vec3 position{0.0f};
    float bounding_radius = 0.0f;
    std::string queue_id;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        auto h = ctx.processor.get_handle(id);
        if (h == render_cmd::k_invalid_handle)
            return;

        auto* object_data = ctx.processor.resolve_object(h);
        if (!object_data)
            return;

        auto* mesh_data = ctx.processor.resolve_mesh(mesh_handle);
        auto* mat_data = ctx.processor.resolve_material(material_handle);

        if (!mesh_data || !mat_data)
            return;

        ctx.loader.update_object(*object_data, *mat_data, *mesh_data, transform, normal_matrix,
                                 position);

        object_data->gpu_data.bounding_radius = bounding_radius;

        auto new_rqid = std::move(queue_id);
        if (new_rqid != object_data->queue_id)
        {
            ctx.vr.remove_from_drawing(object_data);
            object_data->queue_id = std::move(new_rqid);
            ctx.vr.schedule_to_drawing(object_data);
        }

        ctx.vr.schedule_game_data_gpu_upload(object_data);
    }
};

struct destroy_object_cmd : render_cmd::render_command_base
{
    utils::id id;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        auto h = ctx.processor.get_handle(id);
        if (h == render_cmd::k_invalid_handle)
            return;

        auto* object_data = ctx.processor.resolve_object(h);
        if (object_data)
        {
            ctx.vr.remove_from_drawing(object_data);
            ctx.vr.get_cache().objects.release(object_data);
        }
        ctx.processor.erase_object(h);
        ctx.processor.erase_handle(id);
    }
};

// ============================================================================
// Render commands — lights
// ============================================================================

enum class light_kind : uint8_t
{
    directional,
    point,
    spot
};

struct create_light_cmd : render_cmd::render_command_base
{
    utils::id id;
    light_kind kind = light_kind::point;
    glm::vec3 position{0.0f};
    glm::vec3 ambient{0.0f};
    glm::vec3 diffuse{0.0f};
    glm::vec3 specular{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float radius = 0.0f;
    float cut_off = 0.0f;
    float outer_cut_off = 0.0f;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        auto h = ctx.processor.ensure_handle(id);

        if (kind == light_kind::directional)
        {
            auto* rh = ctx.vr.get_cache().directional_lights.alloc(id);
            rh->gpu_data.ambient = ambient;
            rh->gpu_data.diffuse = diffuse;
            rh->gpu_data.specular = specular;
            rh->gpu_data.direction = direction;
            ctx.processor.store_dir_light(h, rh);
            ctx.vr.schedule_directional_light_data_gpu_upload(rh);
        }
        else
        {
            auto lt = (kind == light_kind::spot) ? render::light_type::spot
                                                 : render::light_type::point;
            auto* rh = ctx.vr.get_cache().universal_lights.alloc(id, lt);
            rh->gpu_data.position = position;
            rh->gpu_data.ambient = ambient;
            rh->gpu_data.diffuse = diffuse;
            rh->gpu_data.specular = specular;
            rh->gpu_data.radius = radius;
            rh->gpu_data.direction = direction;
            rh->gpu_data.cut_off = cut_off;
            rh->gpu_data.outer_cut_off = outer_cut_off;
            ctx.processor.store_univ_light(h, rh);
            ctx.vr.schedule_universal_light_data_gpu_upload(rh);
        }
    }
};

struct update_light_cmd : render_cmd::render_command_base
{
    utils::id id;
    light_kind kind = light_kind::point;
    glm::vec3 position{0.0f};
    glm::vec3 ambient{0.0f};
    glm::vec3 diffuse{0.0f};
    glm::vec3 specular{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float radius = 0.0f;
    float cut_off = 0.0f;
    float outer_cut_off = 0.0f;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        auto h = ctx.processor.get_handle(id);
        if (h == render_cmd::k_invalid_handle)
            return;

        if (kind == light_kind::directional)
        {
            auto* rh = ctx.processor.resolve_dir_light(h);
            if (!rh)
                return;

            rh->gpu_data.ambient = ambient;
            rh->gpu_data.diffuse = diffuse;
            rh->gpu_data.specular = specular;
            rh->gpu_data.direction = direction;
            ctx.vr.schedule_directional_light_data_gpu_upload(rh);
        }
        else
        {
            auto* rh = ctx.processor.resolve_univ_light(h);
            if (!rh)
                return;

            rh->gpu_data.position = position;
            rh->gpu_data.ambient = ambient;
            rh->gpu_data.diffuse = diffuse;
            rh->gpu_data.specular = specular;
            rh->gpu_data.radius = radius;
            rh->gpu_data.direction = direction;
            rh->gpu_data.cut_off = cut_off;
            rh->gpu_data.outer_cut_off = outer_cut_off;
            ctx.vr.schedule_universal_light_data_gpu_upload(rh);
        }
    }
};

struct destroy_light_cmd : render_cmd::render_command_base
{
    utils::id id;
    light_kind kind = light_kind::point;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        auto h = ctx.processor.get_handle(id);
        if (h == render_cmd::k_invalid_handle)
            return;

        if (kind == light_kind::directional)
        {
            auto* rh = ctx.processor.resolve_dir_light(h);
            if (rh)
            {
                ctx.vr.get_cache().directional_lights.release(rh);
            }
            ctx.processor.erase_dir_light(h);
        }
        else
        {
            auto* rh = ctx.processor.resolve_univ_light(h);
            if (rh)
            {
                ctx.vr.get_cache().universal_lights.release(rh);
            }
            ctx.processor.erase_univ_light(h);
        }

        ctx.processor.erase_handle(id);
    }
};

// ============================================================================
// Command builders — mesh_component
// ============================================================================

result_code
mesh_component__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto rc = root::game_object_component__cmd_builder(ctx);
    KRG_return_nok(rc);

    auto& moc = ctx.obj->asr<base::mesh_component>();

    if (!moc.get_material() || !moc.get_mesh())
    {
        return result_code::failed;
    }

    rc = ctx.rb->render_cmd_build(*moc.get_material(), ctx.flag);
    KRG_return_nok(rc);

    rc = ctx.rb->render_cmd_build(*moc.get_mesh(), ctx.flag);
    KRG_return_nok(rc);

    moc.update_matrix();

    // Use model-side bounding radius (computed during mesh build)
    float base_radius = moc.get_mesh()->get_bounding_radius();
    moc.set_base_bounding_radius(base_radius);

    auto scale = moc.get_scale();
    float max_scale = glm::max(glm::max(glm::abs(scale.x), glm::abs(scale.y)), glm::abs(scale.z));
    float scaled_radius = base_radius * max_scale;

    auto existing_handle = ctx.rb->get_processor().get_handle(moc.get_id());
    auto mesh_handle = ctx.rb->get_processor().ensure_handle(moc.get_mesh()->get_id());
    auto mat_handle = ctx.rb->get_processor().ensure_handle(moc.get_material()->get_id());
    auto new_rqid = render_bridge::make_qid_from_model(*moc.get_material(), *moc.get_mesh());

    if (existing_handle == render_cmd::k_invalid_handle)
    {
        ctx.rb->get_processor().ensure_handle(moc.get_id());

        auto* cmd = ctx.rb->alloc_cmd<create_object_cmd>();
        cmd->id = moc.get_id();
        cmd->mesh_handle = mesh_handle;
        cmd->material_handle = mat_handle;
        cmd->transform = moc.get_transform_matrix();
        cmd->normal_matrix = moc.get_normal_matrix();
        cmd->position = glm::vec3(moc.get_world_position());
        cmd->bounding_radius = scaled_radius;
        cmd->bone_count = 0;
        cmd->queue_id = new_rqid;

        ctx.rb->enqueue_cmd(cmd);
    }
    else
    {
        auto* cmd = ctx.rb->alloc_cmd<update_object_cmd>();
        cmd->id = moc.get_id();
        cmd->mesh_handle = mesh_handle;
        cmd->material_handle = mat_handle;
        cmd->transform = moc.get_transform_matrix();
        cmd->normal_matrix = moc.get_normal_matrix();
        cmd->position = glm::vec3(moc.get_world_position());
        cmd->bounding_radius = scaled_radius;
        cmd->queue_id = new_rqid;

        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

result_code
mesh_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& moc = ctx.obj->asr<base::mesh_component>();

    auto mat_model = moc.get_material();

    result_code rc = result_code::nav;
    if (mat_model && is_same_source(*ctx.obj, *mat_model))
    {
        rc = ctx.rb->render_cmd_destroy(*moc.get_material(), ctx.flag);
        KRG_return_nok(rc);
    }

    auto mesh_model = moc.get_mesh();

    if (mesh_model && is_same_source(*ctx.obj, *mesh_model))
    {
        rc = ctx.rb->render_cmd_destroy(*moc.get_mesh(), ctx.flag);
        KRG_return_nok(rc);
    }

    if (ctx.rb->get_processor().get_handle(moc.get_id()) != render_cmd::k_invalid_handle)
    {
        auto* cmd = ctx.rb->alloc_cmd<destroy_object_cmd>();
        cmd->id = moc.get_id();
        ctx.rb->enqueue_cmd(cmd);
    }

    rc = root::game_object_component__cmd_destroyer(ctx);
    KRG_return_nok(rc);

    return result_code::ok;
}

/*===============================*/

result_code
directional_light_component__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto& lc_model = ctx.obj->asr<base::directional_light_component>();

    auto existing_handle = ctx.rb->get_processor().get_handle(lc_model.get_id());

    if (existing_handle == render_cmd::k_invalid_handle)
    {
        ctx.rb->get_processor().ensure_handle(lc_model.get_id());

        auto* cmd = ctx.rb->alloc_cmd<create_light_cmd>();
        cmd->id = lc_model.get_id();
        cmd->kind = light_kind::directional;
        cmd->ambient = lc_model.get_ambient();
        cmd->diffuse = lc_model.get_diffuse();
        cmd->specular = lc_model.get_specular();
        cmd->direction = lc_model.get_direction();

        ctx.rb->enqueue_cmd(cmd);
    }
    else
    {
        auto* cmd = ctx.rb->alloc_cmd<update_light_cmd>();
        cmd->id = lc_model.get_id();
        cmd->kind = light_kind::directional;
        cmd->ambient = lc_model.get_ambient();
        cmd->diffuse = lc_model.get_diffuse();
        cmd->specular = lc_model.get_specular();
        cmd->direction = lc_model.get_direction();

        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

result_code
directional_light_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& plc_model = ctx.obj->asr<base::directional_light_component>();
    if (ctx.rb->get_processor().get_handle(plc_model.get_id()) != render_cmd::k_invalid_handle)
    {
        auto* cmd = ctx.rb->alloc_cmd<destroy_light_cmd>();
        cmd->id = plc_model.get_id();
        cmd->kind = light_kind::directional;
        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

/*===============================*/

result_code
spot_light_component__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto& lc_model = ctx.obj->asr<base::spot_light_component>();

    auto existing_handle = ctx.rb->get_processor().get_handle(lc_model.get_id());

    if (existing_handle == render_cmd::k_invalid_handle)
    {
        ctx.rb->get_processor().ensure_handle(lc_model.get_id());

        auto* cmd = ctx.rb->alloc_cmd<create_light_cmd>();
        cmd->id = lc_model.get_id();
        cmd->kind = light_kind::spot;
        cmd->position = lc_model.get_world_position();
        cmd->ambient = lc_model.get_ambient();
        cmd->diffuse = lc_model.get_diffuse();
        cmd->specular = lc_model.get_specular();
        cmd->radius = lc_model.get_radius();
        cmd->direction = lc_model.get_direction();
        cmd->cut_off = glm::cos(glm::radians(lc_model.get_cut_off()));
        cmd->outer_cut_off = glm::cos(glm::radians(lc_model.get_outer_cut_off()));

        ctx.rb->enqueue_cmd(cmd);
    }
    else
    {
        auto* cmd = ctx.rb->alloc_cmd<update_light_cmd>();
        cmd->id = lc_model.get_id();
        cmd->kind = light_kind::spot;
        cmd->position = lc_model.get_world_position();
        cmd->ambient = lc_model.get_ambient();
        cmd->diffuse = lc_model.get_diffuse();
        cmd->specular = lc_model.get_specular();
        cmd->radius = lc_model.get_radius();
        cmd->direction = lc_model.get_direction();
        cmd->cut_off = glm::cos(glm::radians(lc_model.get_cut_off()));
        cmd->outer_cut_off = glm::cos(glm::radians(lc_model.get_outer_cut_off()));

        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

result_code
spot_light_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& slc_model = ctx.obj->asr<base::spot_light_component>();

    if (ctx.rb->get_processor().get_handle(slc_model.get_id()) != render_cmd::k_invalid_handle)
    {
        auto* cmd = ctx.rb->alloc_cmd<destroy_light_cmd>();
        cmd->id = slc_model.get_id();
        cmd->kind = light_kind::spot;
        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

/*===============================*/

result_code
point_light_component__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto& lc_model = ctx.obj->asr<base::point_light_component>();

    auto existing_handle = ctx.rb->get_processor().get_handle(lc_model.get_id());

    if (existing_handle == render_cmd::k_invalid_handle)
    {
        ctx.rb->get_processor().ensure_handle(lc_model.get_id());

        auto* cmd = ctx.rb->alloc_cmd<create_light_cmd>();
        cmd->id = lc_model.get_id();
        cmd->kind = light_kind::point;
        cmd->position = lc_model.get_world_position();
        cmd->ambient = lc_model.get_ambient();
        cmd->diffuse = lc_model.get_diffuse();
        cmd->specular = lc_model.get_specular();
        cmd->radius = lc_model.get_radius();

        ctx.rb->enqueue_cmd(cmd);
    }
    else
    {
        auto* cmd = ctx.rb->alloc_cmd<update_light_cmd>();
        cmd->id = lc_model.get_id();
        cmd->kind = light_kind::point;
        cmd->position = lc_model.get_world_position();
        cmd->ambient = lc_model.get_ambient();
        cmd->diffuse = lc_model.get_diffuse();
        cmd->specular = lc_model.get_specular();
        cmd->radius = lc_model.get_radius();

        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

result_code
point_light_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& plc_model = ctx.obj->asr<base::point_light_component>();
    if (ctx.rb->get_processor().get_handle(plc_model.get_id()) != render_cmd::k_invalid_handle)
    {
        auto* cmd = ctx.rb->alloc_cmd<destroy_light_cmd>();
        cmd->id = plc_model.get_id();
        cmd->kind = light_kind::point;
        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

/*===============================*/

result_code
animated_mesh_component__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto rc = root::game_object_component__cmd_builder(ctx);
    KRG_return_nok(rc);

    auto& amc = ctx.obj->asr<base::animated_mesh_component>();

    if (amc.get_gltf().size() == 0)
    {
        return result_code::failed;
    }

    auto gltf_path = amc.get_gltf().get_file();
    auto path_str = gltf_path.str();

    std::string file_name_part, ext_part;
    gltf_path.parse_file_name_and_ext(file_name_part, ext_part);
    auto stem = file_name_part;

    auto dir_path = gltf_path.parent();

    auto skeleton_id = AID(stem);
    auto mesh_id = AID(stem + "_skinned_mesh");

    auto& anim_sys = glob::glob_state().getr_animation_system();

    if (!anim_sys.get_skeleton(skeleton_id))
    {
        auto ozz_dir = dir_path;
        auto skel_test = (dir_path / (stem + "_skeleton.ozz"));
        if (!skel_test.exists())
        {
            auto& rl = glob::glob_state().getr_resource_locator();
            auto pkg_root = rl.resource_dir(category::packages);
            bool found = false;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(pkg_root.fs()))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }
                if (entry.path().filename().generic_string() == stem + "_skeleton.ozz")
                {
                    ozz_dir = utils::path(entry.path().parent_path());
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                ALOG_ERROR("Cannot find {}_skeleton.ozz in packages", stem);
                return result_code::failed;
            }
        }

        auto skel_path = (ozz_dir / (stem + "_skeleton.ozz")).str();
        ozz::animation::Skeleton ozz_skeleton;
        if (!animation::ozz_loader::load_skeleton(skel_path, ozz_skeleton))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }

        auto dir_str = ozz_dir.str();
        std::string skel_suffix = "_skeleton.ozz";

        for (const auto& entry : std::filesystem::directory_iterator(dir_str))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            auto fname = entry.path().filename().generic_string();
            if (fname.size() <= stem.size() + 1)
            {
                continue;
            }

            if (fname.substr(0, stem.size() + 1) != stem + "_")
            {
                continue;
            }

            if (fname.size() < 4 || fname.substr(fname.size() - 4) != ".ozz")
            {
                continue;
            }

            if (fname.size() >= skel_suffix.size() &&
                fname.substr(fname.size() - skel_suffix.size()) == skel_suffix)
            {
                continue;
            }

            auto clip_name = fname.substr(stem.size() + 1, fname.size() - stem.size() - 1 - 4);

            auto anim_path = (ozz_dir / fname).str();
            ozz::animation::Animation ozz_anim;
            if (animation::ozz_loader::load_animation(anim_path, ozz_anim))
            {
                anim_sys.register_animation(skeleton_id, AID(clip_name), std::move(ozz_anim));
            }
        }

        animation::gltf_load_result gltf_result;
        if (!animation::gltf_animation_loader::load(amc.get_gltf(), gltf_result))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }

        std::vector<int32_t> joint_remaps(gltf_result.joint_names.size(), -1);
        auto ozz_names = ozz_skeleton.joint_names();
        for (size_t mesh_bone = 0; mesh_bone < gltf_result.joint_names.size(); ++mesh_bone)
        {
            const auto& name = gltf_result.joint_names[mesh_bone];
            for (int ozz_j = 0; ozz_j < ozz_skeleton.num_joints(); ++ozz_j)
            {
                if (name == ozz_names[ozz_j])
                {
                    joint_remaps[mesh_bone] = ozz_j;
                    break;
                }
            }

            if (joint_remaps[mesh_bone] < 0)
            {
                ALOG_WARN("Joint '{}' in glTF mesh not found in ozz skeleton", name);
            }
        }

        anim_sys.register_skeleton(skeleton_id, std::move(ozz_skeleton),
                                   std::move(gltf_result.inverse_bind_matrices),
                                   std::move(joint_remaps));

        if (!gltf_result.meshes.empty() &&
            ctx.rb->get_processor().get_handle(mesh_id) == render_cmd::k_invalid_handle)
        {
            auto& gltf_mesh = gltf_result.meshes[0];

            // Compute bounding radius from skinned vertex data
            float max_dist_sq = 0.0f;
            for (const auto& v : gltf_mesh.vertices)
            {
                float dist_sq = glm::dot(v.position, v.position);
                max_dist_sq = std::max(max_dist_sq, dist_sq);
            }
            amc.set_base_bounding_radius(std::sqrt(max_dist_sq));

            auto vert_buf = std::make_shared<utils::buffer>(gltf_mesh.vertices.size() *
                                                            sizeof(gpu::skinned_vertex_data));
            memcpy(vert_buf->data(), gltf_mesh.vertices.data(),
                   gltf_mesh.vertices.size() * sizeof(gpu::skinned_vertex_data));

            auto idx_buf =
                std::make_shared<utils::buffer>(gltf_mesh.indices.size() * sizeof(gpu::uint));
            memcpy(idx_buf->data(), gltf_mesh.indices.data(),
                   gltf_mesh.indices.size() * sizeof(gpu::uint));

            // Reuse root::create_mesh_cmd — but it's file-local there.
            // Instead, use the same pattern inline.
            ctx.rb->get_processor().ensure_handle(mesh_id);

            // We need to create a create_mesh_cmd but it's defined in root's .cpp.
            // For now, define a local skinned mesh command.
            struct create_skinned_mesh_cmd : render_cmd::render_command_base
            {
                utils::id id;
                std::shared_ptr<utils::buffer> vertices;
                std::shared_ptr<utils::buffer> indices;

                void
                execute(render_cmd::render_exec_context& ctx) override
                {
                    auto vbv = vertices->make_view<gpu::skinned_vertex_data>();
                    auto ibv = indices->make_view<gpu::uint>();
                    auto* md = ctx.loader.create_skinned_mesh(id, vbv, ibv);

                    if (md)
                    {
                        auto h = ctx.processor.ensure_handle(id);
                        ctx.processor.store_mesh(h, md);
                    }
                }
            };

            auto* cmd = ctx.rb->alloc_cmd<create_skinned_mesh_cmd>();
            cmd->id = mesh_id;
            cmd->vertices = std::move(vert_buf);
            cmd->indices = std::move(idx_buf);

            ctx.rb->enqueue_cmd(cmd);
        }
    }

    auto mesh_handle = ctx.rb->get_processor().get_handle(mesh_id);
    if (mesh_handle == render_cmd::k_invalid_handle)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    auto* mat_model = amc.get_material();
    if (!mat_model)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    rc = ctx.rb->render_cmd_build(*mat_model, ctx.flag);
    KRG_return_nok(rc);

    auto mat_handle = ctx.rb->get_processor().get_handle(mat_model->get_id());
    if (mat_handle == render_cmd::k_invalid_handle)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    amc.update_matrix();

    auto scale = amc.get_scale();
    float max_scale = glm::max(glm::max(glm::abs(scale.x), glm::abs(scale.y)), glm::abs(scale.z));
    float scaled_radius = amc.get_base_bounding_radius() * max_scale;

    auto* skel = anim_sys.get_skeleton(skeleton_id);
    uint32_t bone_count = skel ? static_cast<uint32_t>(skel->num_joints()) : 0;

    // Compute queue ID from model data (no render-side pointers)
    auto* se = mat_model->get_shader_effect();
    std::string new_rqid;
    if (se && se->m_enable_alpha_support)
    {
        new_rqid = "transparent";
    }
    else
    {
        new_rqid = mat_model->get_id().str() + "::" + mesh_id.str();
    }

    auto existing_handle = ctx.rb->get_processor().get_handle(amc.get_id());

    if (existing_handle == render_cmd::k_invalid_handle)
    {
        ctx.rb->get_processor().ensure_handle(amc.get_id());

        auto* cmd = ctx.rb->alloc_cmd<create_object_cmd>();
        cmd->id = amc.get_id();
        cmd->mesh_handle = mesh_handle;
        cmd->material_handle = mat_handle;
        cmd->transform = amc.get_transform_matrix();
        cmd->normal_matrix = amc.get_normal_matrix();
        cmd->position = glm::vec3(amc.get_world_position());
        cmd->bounding_radius = scaled_radius;
        cmd->bone_count = bone_count;
        cmd->queue_id = new_rqid;

        ctx.rb->enqueue_cmd(cmd);
    }
    else
    {
        auto* cmd = ctx.rb->alloc_cmd<update_object_cmd>();
        cmd->id = amc.get_id();
        cmd->mesh_handle = mesh_handle;
        cmd->material_handle = mat_handle;
        cmd->transform = amc.get_transform_matrix();
        cmd->normal_matrix = amc.get_normal_matrix();
        cmd->position = glm::vec3(amc.get_world_position());
        cmd->bounding_radius = scaled_radius;
        cmd->queue_id = new_rqid;

        ctx.rb->enqueue_cmd(cmd);
    }

    // Animation instance creation — render_data is resolved lazily during tick()
    // via the resolver callback (one frame delay before bone GPU uploads start)
    auto clip_name = amc.get_clip_name();
    if (clip_name.valid() && anim_sys.has_animation(skeleton_id, clip_name))
    {
        auto inst_id = anim_sys.create_instance(amc.get_id(), skeleton_id, clip_name, nullptr);
        amc.set_skeleton_id(skeleton_id);
        amc.set_anim_instance_id(inst_id);
    }

    return result_code::ok;
}

result_code
animated_mesh_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& amc = ctx.obj->asr<base::animated_mesh_component>();

    auto& inst_id = amc.get_animation_instance_id();
    if (inst_id.valid())
    {
        glob::glob_state().getr_animation_system().destroy_instance(inst_id);
    }

    auto mat_model = amc.get_material();
    result_code rc = result_code::nav;
    if (mat_model && is_same_source(*ctx.obj, *mat_model))
    {
        rc = ctx.rb->render_cmd_destroy(*mat_model, ctx.flag);
        KRG_return_nok(rc);
    }

    if (ctx.rb->get_processor().get_handle(amc.get_id()) != render_cmd::k_invalid_handle)
    {
        auto* cmd = ctx.rb->alloc_cmd<destroy_object_cmd>();
        cmd->id = amc.get_id();
        ctx.rb->enqueue_cmd(cmd);
    }

    rc = root::game_object_component__cmd_destroyer(ctx);
    KRG_return_nok(rc);

    return result_code::ok;
}

}  // namespace kryga
