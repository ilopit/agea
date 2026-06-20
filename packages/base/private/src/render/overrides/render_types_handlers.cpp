#include "packages/base/package.base.h"

#include <global_state/global_state.h>
#include <core/model_system.h>
#include <render_translator/render_translator.h>
#include <render_translator/render_convert.h>
#include <render_translator/render_command.h>
#include <core/object_layer_flags.h>
#include <render_translator/render_commands.h>

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
#include "packages/base/model/components/destructible_mesh_component.h"
#include "packages/base/model/components/terrain_component.h"
#include "packages/base/model/assets/destructible_mesh_asset.h"
#include "packages/base/model/assets/terrain_splatmap_material.h"
#include "packages/root/model/game_object.h"

#include <voronoi_fracture/voronoi_fracture.h>

#include <physics/physics_types.h>

#include <physics_translator/physics_translator.h>

#include <utils/buffer.h>

#include "packages/base/model/lights/point_light.h"
#include "packages/base/model/lights/spot_light.h"
#include "packages/base/model/lights/directional_light.h"
#include "packages/base/model/lights/components/directional_light_component.h"
#include "packages/base/model/lights/components/spot_light_component.h"
#include "packages/base/model/lights/components/point_light_component.h"

#include "packages/base/model/mesh_object.h"

#include <core/level.h>
#include <core/lightmap_manifest.h>

#include <core/reflection/reflection_type.h>
#include <core/reflection/property_utils.h>
#include <core/caches/cache_set.h>
#include <core/object_constructor.h>
#include <core/package.h>
#include <core/reflection/reflection_type_utils.h>
#include <core/level.h>

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
#include <vulkan_render/render_system.h>
#include <vulkan_render/kryga_render.h>

#include <animation/gltf_animation_loader.h>
#include <animation/ozz_loader.h>
#include <animation/animation_system.h>

#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>

#include <vfs/vfs.h>

#include <utils/buffer.h>
#include <utils/path.h>
#include <utils/kryga_log.h>
#include <utils/string_utility.h>
#include <utils/dynamic_object_builder.h>

#include <filesystem>
#include <cmath>
#include <limits>
#include <vector>

namespace kryga
{

bool
is_same_source(root::smart_object& obj, root::smart_object& sub_obj)
{
    // Require a NON-null shared owner. A runtime-grafted component (package=null,
    // level=L) merely references shared CDO assets (package=null, level=null);
    // matching on null==null would treat the shared asset as owned and recurse a
    // render-destroy into it — which asserts (CDOs must not be render-destroyed).
    return (obj.get_package() && obj.get_package() == sub_obj.get_package()) ||
           (obj.get_level() && obj.get_level() == sub_obj.get_level());
}

// All render command structs (create/update/destroy object, create_chunk_mesh,
// destroy_mesh, light_kind + create/update/destroy/select light) now live in
// render_translator/render_commands.h — relocated for central tagged dispatch.
// resolve_lightmap moved with them (it is render-thread-only, used by the object
// command bodies). The builders below emit the commands via ctx.rb->alloc_cmd<T>().

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

    moc.set_base_bounding_radius(moc.get_mesh()->get_bounding_radius());
    moc.set_base_centroid(moc.get_mesh()->get_local_centroid().as_glm());

    auto bounds = moc.get_world_bounds();
    float scaled_radius = bounds.radius;
    glm::vec3 world_sphere_center = bounds.center;

    auto new_rqid = render_convert::make_qid_from_model(*moc.get_material(), *moc.get_mesh());

    // Lightmap binding is resolved on the render thread at execute time from the
    // loader's per-level registry — here we only forward which level this instance
    // belongs to (empty id if none). See create_lightmap_cmd / resolve_lightmap.
    utils::id lm_level_id = moc.get_level() ? moc.get_level()->get_id() : utils::id();

    if (!moc.get_render_built())
    {
        auto oh = ctx.rb->objects_alloc().reserve();
        moc.set_render_object_handle(oh);

        auto* cmd = ctx.rb->alloc_cmd<create_object_cmd>();
        cmd->id = moc.get_id();
        cmd->obj_handle = oh;
        cmd->mesh = moc.get_mesh()->render_handle();
        cmd->material = moc.get_material()->render_handle();
        cmd->transform = moc.get_transform_matrix();
        cmd->normal_matrix = moc.get_normal_matrix();
        cmd->position = glm::vec3(moc.get_world_position());
        cmd->bounding_sphere_center = world_sphere_center;
        cmd->bounding_radius = scaled_radius;
        cmd->bone_count = 0;
        cmd->queue_id = new_rqid;
        cmd->lightmap_level_id = lm_level_id;
        cmd->layer_flags = moc.get_layers();

        moc.set_render_built(true);
        ctx.rb->enqueue_cmd(cmd);
    }
    else
    {
        auto* cmd = ctx.rb->alloc_cmd<update_object_cmd>();
        cmd->id = moc.get_id();
        cmd->obj_handle = moc.get_render_object_handle();
        cmd->mesh = moc.get_mesh()->render_handle();
        cmd->material = moc.get_material()->render_handle();
        cmd->transform = moc.get_transform_matrix();
        cmd->normal_matrix = moc.get_normal_matrix();
        cmd->position = glm::vec3(moc.get_world_position());
        cmd->bounding_sphere_center = world_sphere_center;
        cmd->bounding_radius = scaled_radius;
        cmd->queue_id = new_rqid;
        cmd->lightmap_level_id = lm_level_id;
        cmd->layer_flags = moc.get_layers();

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

    if (moc.get_render_built())
    {
        auto* cmd = ctx.rb->alloc_cmd<destroy_object_cmd>();
        cmd->obj_handle = moc.get_render_object_handle();
        ctx.rb->objects_alloc().free(moc.get_render_object_handle());  // [model thread]
        moc.set_render_object_handle({});
        moc.set_render_built(false);
        ctx.rb->enqueue_cmd(cmd);
    }

    rc = root::game_object_component__cmd_destroyer(ctx);
    KRG_return_nok(rc);

    return result_code::ok;
}

result_code
mesh_component__cmd_transform(reflection::type_context__render_cmd_build& ctx)
{
    auto& m = ctx.obj->asr<base::mesh_component>();

    auto* cmd = ctx.rb->alloc_cmd<update_transform_cmd>();
    cmd->obj_handle = m.get_render_object_handle();
    cmd->transform = m.get_transform_matrix();
    cmd->normal_matrix = m.get_normal_matrix();
    cmd->position = glm::vec3(m.get_world_position());

    auto bounds = m.get_world_bounds();
    cmd->bounding_radius = bounds.radius;
    cmd->bounding_sphere_center = bounds.center;

    ctx.rb->enqueue_cmd(cmd);

    return result_code::ok;
}

/*===============================*/

result_code
directional_light_component__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto& lc_model = ctx.obj->asr<base::directional_light_component>();

    // Handle model-side deselection when this light becomes selected
    if (lc_model.get_selected())
    {
        auto* level = glob::glob_state().getr_model().current_level;
        if (level)
        {
            const auto& prev_id = level->get_selected_directional_light_id();
            if (prev_id != lc_model.get_id() && prev_id.valid())
            {
                auto* prev = level->find_component(prev_id);
                if (prev && prev->castable_to<base::directional_light_component>())
                {
                    auto& prev_dl = prev->asr<base::directional_light_component>();
                    prev_dl.set_selected(false);
                    prev_dl.mark_render_dirty();
                }
            }
            level->set_selected_directional_light(lc_model.get_id());
        }
    }

    if (!lc_model.get_render_built())
    {
        auto h = ctx.rb->dir_lights_alloc().reserve();
        lc_model.set_render_light_handle(h);

        auto* cmd = ctx.rb->alloc_cmd<create_light_cmd>();
        cmd->id = lc_model.get_id();
        cmd->dir_handle = h;
        cmd->kind = light_kind::directional;
        cmd->ambient = lc_model.get_ambient();
        cmd->diffuse = lc_model.get_diffuse();
        cmd->specular = lc_model.get_specular();
        cmd->direction = lc_model.get_direction();

        lc_model.set_render_built(true);
        ctx.rb->enqueue_cmd(cmd);
    }
    else
    {
        auto* cmd = ctx.rb->alloc_cmd<update_light_cmd>();
        cmd->id = lc_model.get_id();
        cmd->dir_handle = lc_model.get_render_light_handle();
        cmd->kind = light_kind::directional;
        cmd->ambient = lc_model.get_ambient();
        cmd->diffuse = lc_model.get_diffuse();
        cmd->specular = lc_model.get_specular();
        cmd->direction = lc_model.get_direction();

        ctx.rb->enqueue_cmd(cmd);
    }

    if (lc_model.get_selected())
    {
        auto* cmd = ctx.rb->alloc_cmd<select_directional_light_cmd>();
        cmd->handle = lc_model.get_render_light_handle();
        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

result_code
directional_light_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& plc_model = ctx.obj->asr<base::directional_light_component>();

    if (plc_model.get_render_built())
    {
        auto h = plc_model.get_render_light_handle();
        auto* cmd = ctx.rb->alloc_cmd<destroy_light_cmd>();
        cmd->dir_handle = h;
        cmd->kind = light_kind::directional;
        ctx.rb->dir_lights_alloc().free(h);  // [model thread] deferred recycle
        plc_model.set_render_built(false);
        plc_model.set_render_light_handle({});
        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

/*===============================*/

result_code
spot_light_component__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto& lc_model = ctx.obj->asr<base::spot_light_component>();

    if (!lc_model.get_render_built())
    {
        auto h = ctx.rb->uni_lights_alloc().reserve();
        lc_model.set_render_light_handle(h);

        auto* cmd = ctx.rb->alloc_cmd<create_light_cmd>();
        cmd->id = lc_model.get_id();
        cmd->uni_handle = h;
        cmd->kind = light_kind::spot;
        cmd->position = lc_model.get_world_position();
        cmd->ambient = lc_model.get_ambient();
        cmd->diffuse = lc_model.get_diffuse();
        cmd->specular = lc_model.get_specular();
        cmd->radius = lc_model.get_radius();
        cmd->direction = lc_model.get_direction();
        cmd->cut_off = glm::cos(glm::radians(lc_model.get_cut_off()));
        cmd->outer_cut_off = glm::cos(glm::radians(lc_model.get_outer_cut_off()));

        lc_model.set_render_built(true);
        ctx.rb->enqueue_cmd(cmd);
    }
    else
    {
        auto* cmd = ctx.rb->alloc_cmd<update_light_cmd>();
        cmd->id = lc_model.get_id();
        cmd->uni_handle = lc_model.get_render_light_handle();
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

    if (slc_model.get_render_built())
    {
        auto h = slc_model.get_render_light_handle();
        auto* cmd = ctx.rb->alloc_cmd<destroy_light_cmd>();
        cmd->uni_handle = h;
        cmd->kind = light_kind::spot;
        ctx.rb->uni_lights_alloc().free(h);  // [model thread] deferred recycle
        slc_model.set_render_built(false);
        slc_model.set_render_light_handle({});
        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

result_code
spot_light_component__cmd_transform(reflection::type_context__render_cmd_build& ctx)
{
    auto& lc = ctx.obj->asr<base::spot_light_component>();

    auto* cmd = ctx.rb->alloc_cmd<update_light_cmd>();
    cmd->id = lc.get_id();
    cmd->uni_handle = lc.get_render_light_handle();
    cmd->kind = light_kind::spot;
    cmd->position = lc.get_world_position();
    cmd->ambient = lc.get_ambient();
    cmd->diffuse = lc.get_diffuse();
    cmd->specular = lc.get_specular();
    cmd->radius = lc.get_radius();
    cmd->direction = lc.get_direction();
    cmd->cut_off = glm::cos(glm::radians(lc.get_cut_off()));
    cmd->outer_cut_off = glm::cos(glm::radians(lc.get_outer_cut_off()));
    ctx.rb->enqueue_cmd(cmd);

    return result_code::ok;
}

/*===============================*/

result_code
point_light_component__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto& lc_model = ctx.obj->asr<base::point_light_component>();

    if (!lc_model.get_render_built())
    {
        auto h = ctx.rb->uni_lights_alloc().reserve();
        lc_model.set_render_light_handle(h);

        auto* cmd = ctx.rb->alloc_cmd<create_light_cmd>();
        cmd->id = lc_model.get_id();
        cmd->uni_handle = h;
        cmd->kind = light_kind::point;
        cmd->position = lc_model.get_world_position();
        cmd->ambient = lc_model.get_ambient();
        cmd->diffuse = lc_model.get_diffuse();
        cmd->specular = lc_model.get_specular();
        cmd->radius = lc_model.get_radius();

        lc_model.set_render_built(true);
        ctx.rb->enqueue_cmd(cmd);
    }
    else
    {
        auto* cmd = ctx.rb->alloc_cmd<update_light_cmd>();
        cmd->id = lc_model.get_id();
        cmd->uni_handle = lc_model.get_render_light_handle();
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

    if (plc_model.get_render_built())
    {
        auto h = plc_model.get_render_light_handle();
        auto* cmd = ctx.rb->alloc_cmd<destroy_light_cmd>();
        cmd->uni_handle = h;
        cmd->kind = light_kind::point;
        ctx.rb->uni_lights_alloc().free(h);  // [model thread] deferred recycle
        plc_model.set_render_built(false);
        plc_model.set_render_light_handle({});
        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

result_code
point_light_component__cmd_transform(reflection::type_context__render_cmd_build& ctx)
{
    auto& lc = ctx.obj->asr<base::point_light_component>();

    auto* cmd = ctx.rb->alloc_cmd<update_light_cmd>();
    cmd->id = lc.get_id();
    cmd->uni_handle = lc.get_render_light_handle();
    cmd->kind = light_kind::point;
    cmd->position = lc.get_world_position();
    cmd->ambient = lc.get_ambient();
    cmd->diffuse = lc.get_diffuse();
    cmd->specular = lc.get_specular();
    cmd->radius = lc.get_radius();
    ctx.rb->enqueue_cmd(cmd);

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
            auto& vfs = glob::glob_state().getr_vfs();
            auto pkg_root_rp = vfs.real_path(vfs::rid("data://packages"));
            bool found = false;
            for (const auto& entry :
                 std::filesystem::recursive_directory_iterator(pkg_root_rp.value()))
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

        anim_sys.register_skeleton(skeleton_id,
                                   std::move(ozz_skeleton),
                                   std::move(gltf_result.inverse_bind_matrices),
                                   std::move(joint_remaps));

        if (!gltf_result.meshes.empty() && !anim_sys.has_skinned_mesh(skeleton_id))
        {
            auto& gltf_mesh = gltf_result.meshes[0];

            glm::vec3 vmin{std::numeric_limits<float>::max()};
            glm::vec3 vmax{std::numeric_limits<float>::lowest()};
            for (const auto& v : gltf_mesh.vertices)
            {
                vmin = glm::min(vmin, v.position);
                vmax = glm::max(vmax, v.position);
            }
            glm::vec3 amc_centroid = (vmin + vmax) * 0.5f;
            float max_dc_sq = 0.0f;
            for (const auto& v : gltf_mesh.vertices)
            {
                glm::vec3 d = v.position - amc_centroid;
                max_dc_sq = std::max(max_dc_sq, glm::dot(d, d));
            }
            amc.set_base_bounding_radius(std::sqrt(max_dc_sq));
            amc.set_base_centroid(amc_centroid);

            auto vert_buf = std::make_shared<utils::buffer>(gltf_mesh.vertices.size() *
                                                            sizeof(gpu::skinned_vertex_data));
            memcpy(vert_buf->data(),
                   gltf_mesh.vertices.data(),
                   gltf_mesh.vertices.size() * sizeof(gpu::skinned_vertex_data));

            auto idx_buf =
                std::make_shared<utils::buffer>(gltf_mesh.indices.size() * sizeof(gpu::uint));
            memcpy(idx_buf->data(),
                   gltf_mesh.indices.data(),
                   gltf_mesh.indices.size() * sizeof(gpu::uint));

            anim_sys.set_skinned_mesh_created(skeleton_id);

            // Reserve the shared skinned-mesh slot model-side and park it on the
            // skeleton (the dedup home) so every instance draws by the same handle.
            auto skinned_handle = ctx.rb->meshes_alloc().reserve();
            anim_sys.set_skinned_mesh_handle(skeleton_id, skinned_handle);

            // create_skinned_mesh_cmd now lives in render_translator/render_commands.h.

            auto* cmd = ctx.rb->alloc_cmd<create_skinned_mesh_cmd>();
            cmd->id = mesh_id;
            cmd->handle = skinned_handle;
            cmd->vertices = std::move(vert_buf);
            cmd->indices = std::move(idx_buf);

            ctx.rb->enqueue_cmd(cmd);
        }
    }

    auto* mat_model = amc.get_material();
    if (!mat_model)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    rc = ctx.rb->render_cmd_build(*mat_model, ctx.flag);
    KRG_return_nok(rc);

    amc.update_matrix();

    auto amc_bounds = amc.get_world_bounds();
    float scaled_radius = amc_bounds.radius;
    glm::vec3 amc_world_sphere_center = amc_bounds.center;

    auto* skel = anim_sys.get_skeleton(skeleton_id);
    uint32_t bone_count = skel ? static_cast<uint32_t>(skel->num_joints()) : 0;

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

    if (!amc.get_render_built())
    {
        auto oh = ctx.rb->objects_alloc().reserve();
        amc.set_render_object_handle(oh);

        auto* cmd = ctx.rb->alloc_cmd<create_object_cmd>();
        cmd->id = amc.get_id();
        cmd->obj_handle = oh;
        cmd->mesh = anim_sys.get_skinned_mesh_handle(skeleton_id);
        cmd->material = mat_model->render_handle();
        cmd->transform = amc.get_transform_matrix();
        cmd->normal_matrix = amc.get_normal_matrix();
        cmd->position = glm::vec3(amc.get_world_position());
        cmd->bounding_sphere_center = amc_world_sphere_center;
        cmd->bounding_radius = scaled_radius;
        cmd->bone_count = bone_count;
        cmd->queue_id = new_rqid;

        amc.set_render_built(true);
        ctx.rb->enqueue_cmd(cmd);
    }
    else
    {
        auto* cmd = ctx.rb->alloc_cmd<update_object_cmd>();
        cmd->id = amc.get_id();
        cmd->obj_handle = amc.get_render_object_handle();
        cmd->mesh = anim_sys.get_skinned_mesh_handle(skeleton_id);
        cmd->material = mat_model->render_handle();
        cmd->transform = amc.get_transform_matrix();
        cmd->normal_matrix = amc.get_normal_matrix();
        cmd->position = glm::vec3(amc.get_world_position());
        cmd->bounding_sphere_center = amc_world_sphere_center;
        cmd->bounding_radius = scaled_radius;
        cmd->queue_id = new_rqid;

        ctx.rb->enqueue_cmd(cmd);
    }

    auto clip_name = amc.get_clip_name();
    if (clip_name.valid() && anim_sys.has_animation(skeleton_id, clip_name))
    {
        auto inst_id = anim_sys.create_instance(
            amc.get_id(), skeleton_id, clip_name, amc.get_render_object_handle());
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

    if (amc.get_render_built())
    {
        auto* cmd = ctx.rb->alloc_cmd<destroy_object_cmd>();
        cmd->obj_handle = amc.get_render_object_handle();
        ctx.rb->objects_alloc().free(amc.get_render_object_handle());  // [model thread]
        amc.set_render_object_handle({});
        amc.set_render_built(false);
        ctx.rb->enqueue_cmd(cmd);
    }

    rc = root::game_object_component__cmd_destroyer(ctx);
    KRG_return_nok(rc);

    return result_code::ok;
}

// ============================================================================
// Command builders — destructible_mesh_component
// ============================================================================

namespace
{

utils::id
chunk_mesh_id_for(const utils::id& component_id, uint32_t i)
{
    return AID(std::string(component_id.cstr()) + "::chunk_mesh_" + std::to_string(i));
}

utils::id
chunk_render_id_for(const utils::id& component_id, uint32_t i)
{
    return AID(std::string(component_id.cstr()) + "::chunk_obj_" + std::to_string(i));
}

}  // namespace

result_code
destructible_mesh_component__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto rc = root::game_object_component__cmd_builder(ctx);
    KRG_return_nok(rc);

    auto& dmc = ctx.obj->asr<base::destructible_mesh_component>();

    if (dmc.get_disposed())
    {
        // Lifetime elapsed — nothing to render any more.
        return result_code::ok;
    }

    auto* asset = dmc.get_asset();
    if (!asset || !asset->get_source_mesh() || !asset->get_material())
    {
        return result_code::failed;
    }

    auto* source_mesh = asset->get_source_mesh();
    auto* material = asset->get_material();

    rc = ctx.rb->render_cmd_build(*material, ctx.flag);
    KRG_return_nok(rc);

    rc = ctx.rb->render_cmd_build(*source_mesh, ctx.flag);
    KRG_return_nok(rc);

    dmc.update_matrix();

    float base_radius = source_mesh->get_bounding_radius();
    dmc.set_base_bounding_radius(base_radius);

    auto xform = dmc.get_transform_matrix();
    glm::vec3 scale(glm::length(glm::vec3(xform[0])),
                    glm::length(glm::vec3(xform[1])),
                    glm::length(glm::vec3(xform[2])));
    float max_scale = glm::max(glm::max(scale.x, scale.y), scale.z);
    float scaled_radius = base_radius * max_scale;

    auto new_rqid = render_convert::make_qid_from_model(*material, *source_mesh);

    // Physics is reached only through the bridge now: this builder runs on the model
    // thread, so it emits intents (register / unregister) and READS the published
    // snapshot (broken / expired / chunk transforms) — it never touches the Jolt world
    // (owned by the physics thread).
    auto& pb = glob::glob_state().getr_physics_translator();

    // ------------------------------------------------------------------
    // First build: pre-fracture, upload chunk meshes, register physics,
    // create the source-mesh render object for the unbroken state.
    // ------------------------------------------------------------------
    if (!dmc.get_render_built())
    {
        auto& chunk_shapes = dmc.get_chunk_shapes();

        auto vbuf = source_mesh->get_vertices_buffer().make_view<gpu::vertex_data>();
        auto ibuf = source_mesh->get_indices_buffer().make_view<gpu::uint>();

        voronoi_fracture::fracture_params fparams{};
        fparams.seed = asset->get_fracture_seed();
        fparams.cell_count = asset->get_cell_count();
        fparams.fill = voronoi_fracture::fill_mode::convex;
        fparams.roughness = 0.15f;

        auto fresult = voronoi_fracture::fracture_mesh(vbuf.as(),
                                                       static_cast<uint32_t>(vbuf.size()),
                                                       ibuf.as(),
                                                       static_cast<uint32_t>(ibuf.size()),
                                                       fparams);

        chunk_shapes.clear();
        chunk_shapes.reserve(fresult.chunks.size());
        dmc.get_chunk_mesh_ids().clear();
        dmc.get_chunk_mesh_ids().reserve(fresult.chunks.size());
        dmc.get_chunk_mesh_handles().clear();
        dmc.get_chunk_mesh_handles().reserve(fresult.chunks.size());

        for (uint32_t i = 0; i < fresult.chunks.size(); ++i)
        {
            const auto& ch = fresult.chunks[i];

            physics::chunk_shape cs;
            cs.aabb_min = ch.aabb_min;
            cs.aabb_max = ch.aabb_max;
            cs.seed_point = ch.seed_point;
            cs.hull_points.reserve(ch.vertices.size());
            for (const auto& v : ch.vertices)
            {
                cs.hull_points.push_back(v.position);
            }
            chunk_shapes.push_back(std::move(cs));

            utils::id chunk_mesh_id = chunk_mesh_id_for(dmc.get_id(), i);
            dmc.get_chunk_mesh_ids().push_back(chunk_mesh_id);
            auto chunk_handle = ctx.rb->meshes_alloc().reserve();
            dmc.get_chunk_mesh_handles().push_back(chunk_handle);

            auto vb =
                std::make_shared<utils::buffer>(ch.vertices.size() * sizeof(gpu::vertex_data));
            std::memcpy(
                vb->data(), ch.vertices.data(), ch.vertices.size() * sizeof(gpu::vertex_data));

            auto ib = std::make_shared<utils::buffer>(ch.indices.size() * sizeof(gpu::uint));
            std::memcpy(ib->data(), ch.indices.data(), ch.indices.size() * sizeof(gpu::uint));

            auto* cmd = ctx.rb->alloc_cmd<create_chunk_mesh_cmd>();
            cmd->id = chunk_mesh_id;
            cmd->handle = chunk_handle;
            cmd->vertices = std::move(vb);
            cmd->indices = std::move(ib);
            ctx.rb->enqueue_cmd(cmd);
        }

        ALOG_INFO("destructible_mesh {}: pre-fractured into {} chunks",
                  dmc.get_id().str(),
                  chunk_shapes.size());

        // Mint the handle producer-side and emit a single register intent (params +
        // initial transform folded in). The physics thread copies the chunk shapes
        // when it processes the message — chunk_shapes lives in the component, so the
        // borrow is valid until then.
        auto handle = pb.register_destructible(chunk_shapes,
                                               asset->get_damage_threshold(),
                                               asset->get_lifetime(),
                                               asset->get_explosion_strength(),
                                               dmc.get_transform_matrix());
        dmc.set_physics_handle(handle);

        auto src_oh = ctx.rb->objects_alloc().reserve();
        dmc.set_render_object_handle(src_oh);

        auto* cmd = ctx.rb->alloc_cmd<create_object_cmd>();
        cmd->id = dmc.get_id();
        cmd->obj_handle = src_oh;
        cmd->mesh = source_mesh->render_handle();
        cmd->material = material->render_handle();
        cmd->transform = dmc.get_transform_matrix();
        cmd->normal_matrix = dmc.get_normal_matrix();
        cmd->position = glm::vec3(dmc.get_world_position());
        cmd->bounding_sphere_center = cmd->position;
        cmd->bounding_radius = scaled_radius;
        cmd->bone_count = 0;
        cmd->queue_id = new_rqid;
        cmd->layer_flags = dmc.get_layers();
        ctx.rb->enqueue_cmd(cmd);

        dmc.set_render_built(true);
        return result_code::ok;
    }

    // ------------------------------------------------------------------
    // Subsequent builds: advance based on the published physics snapshot.
    // ------------------------------------------------------------------
    const destructible_state* st = nullptr;
    bool broken = false;
    bool expired = false;
    if (dmc.get_physics_handle().valid())
    {
        st = pb.get_state(dmc.get_physics_handle());
        if (st)
        {
            broken = st->broken;
            expired = st->expired;
        }
    }

    // Transition to broken — destroy source object, create per-chunk render
    // objects at their current physics transforms.
    if (broken && !dmc.get_chunks_rendering() && !expired)
    {
        {
            auto* cmd = ctx.rb->alloc_cmd<destroy_object_cmd>();
            cmd->obj_handle = dmc.get_render_object_handle();
            ctx.rb->objects_alloc().free(dmc.get_render_object_handle());  // [model thread]
            dmc.set_render_object_handle({});
            ctx.rb->enqueue_cmd(cmd);
        }

        glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), scale);

        dmc.get_chunk_render_ids().clear();
        dmc.get_chunk_render_ids().reserve(dmc.get_chunk_shapes().size());
        dmc.get_chunk_render_handles().clear();
        dmc.get_chunk_render_handles().reserve(dmc.get_chunk_shapes().size());

        for (uint32_t i = 0; i < dmc.get_chunk_shapes().size(); ++i)
        {
            utils::id chunk_render_id = chunk_render_id_for(dmc.get_id(), i);
            dmc.get_chunk_render_ids().push_back(chunk_render_id);
            auto chunk_oh = ctx.rb->objects_alloc().reserve();
            dmc.get_chunk_render_handles().push_back(chunk_oh);

            const auto& cs = dmc.get_chunk_shapes()[i];
            glm::vec3 far_corner = glm::max(glm::abs(cs.aabb_min), glm::abs(cs.aabb_max));
            float chunk_radius = glm::length(far_corner) * max_scale;

            glm::mat4 xf(1.0f);
            if (st && i < st->chunk_transforms.size())
            {
                xf = st->chunk_transforms[i];
            }
            xf = xf * scale_mat;

            auto* cmd = ctx.rb->alloc_cmd<create_object_cmd>();
            cmd->id = chunk_render_id;
            cmd->obj_handle = chunk_oh;
            cmd->mesh = dmc.get_chunk_mesh_handles()[i];
            cmd->material = material->render_handle();
            cmd->transform = xf;
            cmd->normal_matrix = glm::transpose(glm::inverse(xf));
            cmd->position = glm::vec3(xf[3]);
            cmd->bounding_sphere_center = cmd->position;
            cmd->bounding_radius = chunk_radius;
            cmd->bone_count = 0;
            cmd->queue_id = new_rqid;
            cmd->layer_flags = dmc.get_layers();
            ctx.rb->enqueue_cmd(cmd);
        }

        dmc.set_chunks_rendering(true);
        return result_code::ok;
    }

    // Broken steady — forward chunk transforms from physics.
    if (broken && dmc.get_chunks_rendering() && !expired)
    {
        glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), scale);

        for (uint32_t i = 0; i < dmc.get_chunk_render_handles().size(); ++i)
        {
            glm::mat4 xf(1.0f);
            if (st && i < st->chunk_transforms.size())
            {
                xf = st->chunk_transforms[i];
            }
            xf = xf * scale_mat;

            const auto& cs = dmc.get_chunk_shapes()[i];
            glm::vec3 far_corner = glm::max(glm::abs(cs.aabb_min), glm::abs(cs.aabb_max));
            float chunk_radius = glm::length(far_corner) * max_scale;

            auto* cmd = ctx.rb->alloc_cmd<update_transform_cmd>();
            cmd->obj_handle = dmc.get_chunk_render_handles()[i];
            cmd->transform = xf;
            cmd->normal_matrix = glm::transpose(glm::inverse(xf));
            cmd->position = glm::vec3(xf[3]);
            cmd->bounding_sphere_center = cmd->position;
            cmd->bounding_radius = chunk_radius;
            ctx.rb->enqueue_cmd(cmd);
        }
        return result_code::ok;
    }

    // Expired — tear down chunks + meshes, unregister from physics.
    if (expired && dmc.get_chunks_rendering())
    {
        for (const auto& chunk_render_handle : dmc.get_chunk_render_handles())
        {
            auto* cmd = ctx.rb->alloc_cmd<destroy_object_cmd>();
            cmd->obj_handle = chunk_render_handle;
            ctx.rb->objects_alloc().free(chunk_render_handle);  // [model thread]
            ctx.rb->enqueue_cmd(cmd);
        }
        for (const auto& chunk_mesh_handle : dmc.get_chunk_mesh_handles())
        {
            ctx.rb->meshes_alloc().free(chunk_mesh_handle);  // [model thread]
            auto* cmd = ctx.rb->alloc_cmd<destroy_mesh_cmd>();
            cmd->handle = chunk_mesh_handle;
            ctx.rb->enqueue_cmd(cmd);
        }

        if (dmc.get_physics_handle().valid())
        {
            pb.unregister(dmc.get_physics_handle());
            dmc.set_physics_handle({});
        }

        dmc.get_chunk_render_ids().clear();
        dmc.get_chunk_render_handles().clear();
        dmc.set_chunks_rendering(false);
        dmc.set_disposed(true);
        return result_code::ok;
    }

    // Unbroken — push transform updates for the source mesh.
    if (!broken)
    {
        auto* cmd = ctx.rb->alloc_cmd<update_object_cmd>();
        cmd->id = dmc.get_id();
        cmd->obj_handle = dmc.get_render_object_handle();
        cmd->mesh = source_mesh->render_handle();
        cmd->material = material->render_handle();
        cmd->transform = dmc.get_transform_matrix();
        cmd->normal_matrix = dmc.get_normal_matrix();
        cmd->position = glm::vec3(dmc.get_world_position());
        cmd->bounding_radius = scaled_radius;
        cmd->queue_id = new_rqid;
        cmd->layer_flags = dmc.get_layers();
        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

result_code
destructible_mesh_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& dmc = ctx.obj->asr<base::destructible_mesh_component>();

    auto* asset = dmc.get_asset();

    result_code rc = result_code::nav;
    if (asset)
    {
        if (auto* mat = asset->get_material(); mat && is_same_source(*ctx.obj, *mat))
        {
            rc = ctx.rb->render_cmd_destroy(*mat, ctx.flag);
            KRG_return_nok(rc);
        }

        if (auto* src = asset->get_source_mesh(); src && is_same_source(*ctx.obj, *src))
        {
            rc = ctx.rb->render_cmd_destroy(*src, ctx.flag);
            KRG_return_nok(rc);
        }
    }

    // Destroy any still-live chunk render objects.
    if (dmc.get_chunks_rendering())
    {
        for (const auto& chunk_render_handle : dmc.get_chunk_render_handles())
        {
            auto* cmd = ctx.rb->alloc_cmd<destroy_object_cmd>();
            cmd->obj_handle = chunk_render_handle;
            ctx.rb->objects_alloc().free(chunk_render_handle);  // [model thread]
            ctx.rb->enqueue_cmd(cmd);
        }
        dmc.get_chunk_render_handles().clear();
    }

    // Destroy chunk mesh resources uploaded on first build.
    for (const auto& chunk_mesh_handle : dmc.get_chunk_mesh_handles())
    {
        ctx.rb->meshes_alloc().free(chunk_mesh_handle);  // [model thread]
        auto* cmd = ctx.rb->alloc_cmd<destroy_mesh_cmd>();
        cmd->handle = chunk_mesh_handle;
        ctx.rb->enqueue_cmd(cmd);
    }

    // Destroy the unbroken source mesh render object if it's still alive.
    if (dmc.get_render_built() && !dmc.get_chunks_rendering() && !dmc.get_disposed())
    {
        auto* cmd = ctx.rb->alloc_cmd<destroy_object_cmd>();
        cmd->obj_handle = dmc.get_render_object_handle();
        ctx.rb->objects_alloc().free(dmc.get_render_object_handle());  // [model thread]
        dmc.set_render_object_handle({});
        ctx.rb->enqueue_cmd(cmd);
    }
    dmc.set_render_built(false);

    // Unregister from physics (no-op if expiry already tore it down). Emits an intent
    // to the physics thread and drops the model-side snapshot.
    if (dmc.get_physics_handle().valid())
    {
        glob::glob_state().getr_physics_translator().unregister(dmc.get_physics_handle());
        dmc.set_physics_handle({});
    }

    rc = root::game_object_component__cmd_destroyer(ctx);
    KRG_return_nok(rc);

    return result_code::ok;
}

// ============================================================================
// Command builders — terrain_component
//
// Terrain geometry is *derived data*, not a persisted mesh asset: the builder
// generates a grid mesh from a heightmap (or procedural fbm noise) and uploads
// it under a synthesized id, then renders it through the regular object path
// (create_object_cmd) with a terrain_splatmap_material.
// ============================================================================

namespace
{

utils::id
terrain_mesh_id_for(const utils::id& component_id)
{
    return AID(std::string(component_id.cstr()) + "::terrain_mesh");
}

// Integer hash → [0,1). Deterministic for a given (x, y, seed).
float
terrain_hash(int32_t x, int32_t y, uint32_t seed)
{
    uint32_t h = seed + 0x9E3779B9u;
    h ^= static_cast<uint32_t>(x) * 0x85EBCA6Bu;
    h = (h ^ (h >> 13)) * 0xC2B2AE35u;
    h ^= static_cast<uint32_t>(y) * 0x27D4EB2Fu;
    h = (h ^ (h >> 16)) * 0x165667B1u;
    h ^= h >> 15;
    return static_cast<float>(h & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

float
terrain_value_noise(float x, float y, uint32_t seed)
{
    float fx = std::floor(x);
    float fy = std::floor(y);
    int32_t x0 = static_cast<int32_t>(fx);
    int32_t y0 = static_cast<int32_t>(fy);
    float tx = x - fx;
    float ty = y - fy;

    float v00 = terrain_hash(x0, y0, seed);
    float v10 = terrain_hash(x0 + 1, y0, seed);
    float v01 = terrain_hash(x0, y0 + 1, seed);
    float v11 = terrain_hash(x0 + 1, y0 + 1, seed);

    float ux = tx * tx * (3.0f - 2.0f * tx);
    float uy = ty * ty * (3.0f - 2.0f * ty);

    float a = glm::mix(v00, v10, ux);
    float b = glm::mix(v01, v11, ux);
    return glm::mix(a, b, uy);
}

// fbm in [0,1]. u,v in [0,1] across the terrain.
float
terrain_fbm(float u, float v, const base::terrain_component& tc)
{
    float freq = tc.get_frequency();
    float amp = 1.0f;
    float sum = 0.0f;
    float total_amp = 0.0f;
    uint32_t octaves = glm::clamp<uint32_t>(tc.get_octaves(), 1u, 8u);

    for (uint32_t o = 0; o < octaves; ++o)
    {
        sum += amp * terrain_value_noise(u * freq, v * freq, tc.get_seed() + o * 1013u);
        total_amp += amp;
        freq *= tc.get_lacunarity();
        amp *= tc.get_gain();
    }
    return (total_amp > 0.0f) ? sum / total_amp : 0.0f;
}

// Build the normalized [0,1] height field for the terrain into `heights`
// (row-major, res*res). Returns false on a hard failure.
bool
build_height_field(const base::terrain_component& tc, uint32_t res, std::vector<float>& heights)
{
    heights.assign(static_cast<size_t>(res) * res, 0.0f);

    if (tc.get_source_mode() == base::terrain_source_heightmap)
    {
        auto* tex = tc.get_heightmap();
        if (!tex)
        {
            ALOG_WARN("terrain {}: heightmap source but no texture set — flat terrain",
                      tc.get_id().str());
            return true;
        }

        utils::buffer pixels;
        uint32_t hw = 0;
        uint32_t hh = 0;
        if (!asset_importer::texture_importer::extract_texture_from_buffer(
                tex->get_mutable_base_color(), pixels, hw, hh) ||
            hw == 0 || hh == 0)
        {
            ALOG_ERROR("terrain {}: failed to decode heightmap", tc.get_id().str());
            return false;
        }

        const uint8_t* px = pixels.data();
        for (uint32_t j = 0; j < res; ++j)
        {
            for (uint32_t i = 0; i < res; ++i)
            {
                // Bilinear sample of the red channel at this grid uv.
                float u = (res > 1) ? float(i) / float(res - 1) : 0.0f;
                float v = (res > 1) ? float(j) / float(res - 1) : 0.0f;
                float fx = u * float(hw - 1);
                float fy = v * float(hh - 1);
                uint32_t x0 = static_cast<uint32_t>(fx);
                uint32_t y0 = static_cast<uint32_t>(fy);
                uint32_t x1 = glm::min(x0 + 1, hw - 1);
                uint32_t y1 = glm::min(y0 + 1, hh - 1);
                float tx = fx - float(x0);
                float ty = fy - float(y0);

                auto red = [&](uint32_t x, uint32_t y)
                { return float(px[(size_t(y) * hw + x) * 4]) / 255.0f; };

                float a = glm::mix(red(x0, y0), red(x1, y0), tx);
                float b = glm::mix(red(x0, y1), red(x1, y1), tx);
                heights[size_t(j) * res + i] = glm::mix(a, b, ty);
            }
        }
        return true;
    }

    // Procedural noise.
    for (uint32_t j = 0; j < res; ++j)
    {
        for (uint32_t i = 0; i < res; ++i)
        {
            float u = (res > 1) ? float(i) / float(res - 1) : 0.0f;
            float v = (res > 1) ? float(j) / float(res - 1) : 0.0f;
            heights[size_t(j) * res + i] = terrain_fbm(u, v, tc);
        }
    }
    return true;
}

}  // namespace

result_code
terrain_component__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto rc = root::game_object_component__cmd_builder(ctx);
    KRG_return_nok(rc);

    auto& tc = ctx.obj->asr<base::terrain_component>();

    auto* material = tc.get_material();
    if (!material)
    {
        return result_code::failed;
    }

    rc = ctx.rb->render_cmd_build(*material, ctx.flag);
    KRG_return_nok(rc);

    tc.update_matrix();

    const utils::id mesh_id = terrain_mesh_id_for(tc.get_id());

    // First build: generate + upload the grid, then create the render object.
    if (!tc.get_render_built())
    {
        uint32_t res = glm::clamp<uint32_t>(tc.get_resolution(), 2u, 4096u);
        float size = tc.get_world_size();
        float half = size * 0.5f;
        float h_scale = tc.get_height_scale();
        float cell = (res > 1) ? size / float(res - 1) : size;

        std::vector<float> heights;
        if (!build_height_field(tc, res, heights))
        {
            return result_code::failed;
        }

        const uint32_t vcount = res * res;
        const uint32_t icount = (res - 1) * (res - 1) * 6;

        auto vbuf = std::make_shared<utils::buffer>(size_t(vcount) * sizeof(gpu::vertex_data));
        auto ibuf = std::make_shared<utils::buffer>(size_t(icount) * sizeof(gpu::uint));
        auto vv = vbuf->make_view<gpu::vertex_data>();
        auto iv = ibuf->make_view<gpu::uint>();

        auto height_at = [&](int32_t i, int32_t j) -> float
        {
            i = glm::clamp(i, 0, int32_t(res) - 1);
            j = glm::clamp(j, 0, int32_t(res) - 1);
            return heights[size_t(j) * res + i] * h_scale;
        };

        glm::vec3 vmin{std::numeric_limits<float>::max()};
        glm::vec3 vmax{std::numeric_limits<float>::lowest()};

        for (uint32_t j = 0; j < res; ++j)
        {
            for (uint32_t i = 0; i < res; ++i)
            {
                float h = heights[size_t(j) * res + i] * h_scale;
                glm::vec3 pos{-half + float(i) * cell, h, -half + float(j) * cell};

                // Normal from central differences of neighbor heights (+Y up).
                float hl = height_at(int32_t(i) - 1, int32_t(j));
                float hr = height_at(int32_t(i) + 1, int32_t(j));
                float hd = height_at(int32_t(i), int32_t(j) - 1);
                float hu = height_at(int32_t(i), int32_t(j) + 1);
                glm::vec3 nrm = glm::normalize(glm::vec3(hl - hr, 2.0f * cell, hd - hu));

                gpu::vertex_data vert{};
                vert.position = pos;
                vert.normal = nrm;
                vert.color = {1.0f, 1.0f, 1.0f};
                vert.uv = {(res > 1) ? float(i) / float(res - 1) : 0.0f,
                           (res > 1) ? float(j) / float(res - 1) : 0.0f};
                vert.uv2 = {0.0f, 0.0f};
                vv.at(size_t(j) * res + i) = vert;

                vmin = glm::min(vmin, pos);
                vmax = glm::max(vmax, pos);
            }
        }

        uint32_t k = 0;
        for (uint32_t j = 0; j < res - 1; ++j)
        {
            for (uint32_t i = 0; i < res - 1; ++i)
            {
                uint32_t v0 = j * res + i;
                uint32_t v1 = v0 + 1;
                uint32_t v2 = v0 + res;
                uint32_t v3 = v2 + 1;

                iv.at(k++) = v0;
                iv.at(k++) = v2;
                iv.at(k++) = v1;
                iv.at(k++) = v1;
                iv.at(k++) = v2;
                iv.at(k++) = v3;
            }
        }

        glm::vec3 centroid = (vmin + vmax) * 0.5f;
        float max_d2 = 0.0f;
        for (uint32_t v = 0; v < vcount; ++v)
        {
            glm::vec3 d = vv.at(v).position - centroid;
            max_d2 = std::max(max_d2, glm::dot(d, d));
        }
        tc.set_local_centroid(centroid);
        tc.set_base_bounding_radius(std::sqrt(max_d2));

        // The static collider for this terrain is registered separately by
        // terrain_component__physics_cmd_builder (via physics_translator), not here —
        // render no longer talks to the physics world directly.

        // Handle model: reserve the mesh slot on the model thread and store it on the
        // component; the create_mesh command populates that slot on the render thread.
        auto mesh_handle = ctx.rb->meshes_alloc().reserve();
        tc.set_mesh_handle(mesh_handle);

        auto* mcmd = ctx.rb->alloc_cmd<create_mesh_cmd>();
        mcmd->id = mesh_id;
        mcmd->handle = mesh_handle;
        mcmd->vertices = std::move(vbuf);
        mcmd->indices = std::move(ibuf);
        ctx.rb->enqueue_cmd(mcmd);
    }

    auto scale = tc.get_scale();
    float max_scale = glm::max(glm::max(glm::abs(scale.x), glm::abs(scale.y)), glm::abs(scale.z));
    float scaled_radius = tc.get_base_bounding_radius() * max_scale;

    glm::vec4 world_center4 =
        tc.get_transform_matrix() * glm::vec4(tc.get_local_centroid(), 1.0f);
    glm::vec3 world_sphere_center{world_center4.x, world_center4.y, world_center4.z};

    std::string new_rqid = material->get_id().str() + "::" + mesh_id.str();

    if (!tc.get_render_built())
    {
        auto obj_handle = ctx.rb->objects_alloc().reserve();
        tc.set_render_object_handle(obj_handle);

        auto* cmd = ctx.rb->alloc_cmd<create_object_cmd>();
        cmd->id = tc.get_id();
        cmd->obj_handle = obj_handle;
        cmd->mesh = tc.get_mesh_handle();
        cmd->material = material->render_handle();
        cmd->transform = tc.get_transform_matrix();
        cmd->normal_matrix = tc.get_normal_matrix();
        cmd->position = glm::vec3(tc.get_world_position());
        cmd->bounding_sphere_center = world_sphere_center;
        cmd->bounding_radius = scaled_radius;
        cmd->bone_count = 0;
        cmd->queue_id = new_rqid;
        cmd->layer_flags = tc.get_layers();

        tc.set_render_built(true);
        ctx.rb->enqueue_cmd(cmd);
    }
    else
    {
        auto* cmd = ctx.rb->alloc_cmd<update_object_cmd>();
        cmd->id = tc.get_id();
        cmd->obj_handle = tc.get_render_object_handle();
        cmd->mesh = tc.get_mesh_handle();
        cmd->material = material->render_handle();
        cmd->transform = tc.get_transform_matrix();
        cmd->normal_matrix = tc.get_normal_matrix();
        cmd->position = glm::vec3(tc.get_world_position());
        cmd->bounding_sphere_center = world_sphere_center;
        cmd->bounding_radius = scaled_radius;
        cmd->queue_id = new_rqid;
        cmd->layer_flags = tc.get_layers();

        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

result_code
terrain_component__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& tc = ctx.obj->asr<base::terrain_component>();

    auto* material = tc.get_material();
    if (material && is_same_source(*ctx.obj, *material))
    {
        auto rc = ctx.rb->render_cmd_destroy(*material, ctx.flag);
        KRG_return_nok(rc);
    }

    if (tc.get_render_built())
    {
        auto* ocmd = ctx.rb->alloc_cmd<destroy_object_cmd>();
        ocmd->obj_handle = tc.get_render_object_handle();
        ctx.rb->objects_alloc().free(tc.get_render_object_handle());  // [model thread]
        tc.set_render_object_handle({});
        ctx.rb->enqueue_cmd(ocmd);

        auto* mcmd = ctx.rb->alloc_cmd<destroy_mesh_cmd>();
        mcmd->handle = tc.get_mesh_handle();
        ctx.rb->meshes_alloc().free(tc.get_mesh_handle());  // [model thread]
        tc.set_mesh_handle({});
        ctx.rb->enqueue_cmd(mcmd);

        tc.set_render_built(false);
    }

    // The static collider is torn down by terrain_component__physics_cmd_destroyer
    // (via physics_translator), not here.

    auto rc = root::game_object_component__cmd_destroyer(ctx);
    KRG_return_nok(rc);

    return result_code::ok;
}

result_code
terrain_component__cmd_transform(reflection::type_context__render_cmd_build& ctx)
{
    auto& tc = ctx.obj->asr<base::terrain_component>();

    auto* cmd = ctx.rb->alloc_cmd<update_transform_cmd>();
    cmd->obj_handle = tc.get_render_object_handle();
    cmd->transform = tc.get_transform_matrix();
    cmd->normal_matrix = tc.get_normal_matrix();
    cmd->position = glm::vec3(tc.get_world_position());

    auto scale = tc.get_scale();
    float max_s = glm::max(glm::max(glm::abs(scale.x), glm::abs(scale.y)), glm::abs(scale.z));
    cmd->bounding_radius = tc.get_base_bounding_radius() * max_s;

    glm::vec4 wc = tc.get_transform_matrix() * glm::vec4(tc.get_local_centroid(), 1.0f);
    cmd->bounding_sphere_center = glm::vec3(wc);

    ctx.rb->enqueue_cmd(cmd);

    return result_code::ok;
}

result_code
terrain_component__physics_cmd_builder(reflection::type_context__physics_cmd_build& ctx)
{
    auto rc = root::game_object_component__physics_cmd_builder(ctx);
    KRG_return_nok(rc);

    auto& tc = ctx.obj->asr<base::terrain_component>();

    auto* ps = glob::glob_state().get_physics_system();
    if (!ps)
    {
        return result_code::ok;
    }

    // Static collider, baked once (static v1): if it already exists, leave it. The
    // grid is regenerated here (deterministic from the same params) rather than
    // shared with the render builder, so the two bridges stay independent. The
    // position/index math mirrors terrain_component__cmd_builder exactly so the
    // collider aligns with the rendered surface.
    if (tc.get_physics_handle().valid())
    {
        return result_code::ok;
    }

    tc.update_matrix();

    uint32_t res = glm::clamp<uint32_t>(tc.get_resolution(), 2u, 4096u);
    float size = tc.get_world_size();
    float half = size * 0.5f;
    float h_scale = tc.get_height_scale();
    float cell = (res > 1) ? size / float(res - 1) : size;

    std::vector<float> heights;
    if (!build_height_field(tc, res, heights))
    {
        return result_code::failed;
    }

    const uint32_t vcount = res * res;
    const uint32_t icount = (res - 1) * (res - 1) * 6;

    // Build the collider grid into the component's PERSISTENT mesh, then hand the
    // physics ring a borrowed pointer to it: the processor copies it (create_static_mesh)
    // on the physics thread, and the component owns the storage past the borrow window.
    // The identity is minted by the bridge so the component records it synchronously;
    // the Jolt body is built (and the identity mapped to it) when the command drains.
    physics::static_world_mesh& mesh = tc.collider_mesh();
    mesh.vertices.clear();
    mesh.indices.clear();

    const glm::mat4 xf = tc.get_transform_matrix();
    mesh.vertices.reserve(vcount);
    for (uint32_t j = 0; j < res; ++j)
    {
        for (uint32_t i = 0; i < res; ++i)
        {
            float hh = heights[size_t(j) * res + i] * h_scale;
            glm::vec3 pos{-half + float(i) * cell, hh, -half + float(j) * cell};
            mesh.vertices.push_back(glm::vec3(xf * glm::vec4(pos, 1.0f)));
        }
    }

    mesh.indices.reserve(icount);
    for (uint32_t j = 0; j < res - 1; ++j)
    {
        for (uint32_t i = 0; i < res - 1; ++i)
        {
            uint32_t v0 = j * res + i;
            uint32_t v1 = v0 + 1;
            uint32_t v2 = v0 + res;
            uint32_t v3 = v2 + 1;

            mesh.indices.push_back(v0);
            mesh.indices.push_back(v2);
            mesh.indices.push_back(v1);
            mesh.indices.push_back(v1);
            mesh.indices.push_back(v2);
            mesh.indices.push_back(v3);
        }
    }

    tc.set_physics_handle(ctx.pb->register_static_collider(mesh));

    return result_code::ok;
}

result_code
terrain_component__physics_cmd_destroyer(reflection::type_context__physics_cmd_build& ctx)
{
    auto& tc = ctx.obj->asr<base::terrain_component>();

    if (tc.get_physics_handle().valid())
    {
        ctx.pb->unregister_static_collider(tc.get_physics_handle());
        tc.set_physics_handle({});
    }

    auto rc = root::game_object_component__physics_cmd_destroyer(ctx);
    KRG_return_nok(rc);

    return result_code::ok;
}

}  // namespace kryga
