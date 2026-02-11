#include "packages/base/package.base.h"

#include <global_state/global_state.h>
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

/*===============================*/

result_code
mesh_component__render_loader(reflection::type_context__render& ctx)
{
    auto rc = root::game_object_component__render_loader(ctx);
    KRG_return_nok(rc);

    auto& moc = ctx.obj->asr<base::mesh_component>();

    if (!moc.get_material() || !moc.get_mesh())
    {
        return result_code::failed;
    }

    rc = ctx.rb->render_ctor(*moc.get_material(), ctx.flag);
    KRG_return_nok(rc);

    rc = ctx.rb->render_ctor(*moc.get_mesh(), ctx.flag);
    KRG_return_nok(rc);

    auto mat_data = moc.get_material()->get_material_data();
    auto mesh_data = moc.get_mesh()->get_mesh_data();

    if (!mat_data || !mesh_data)
    {
        return result_code::failed;
    }

    auto object_data = moc.get_render_object_data();

    moc.update_matrix();

    // Calculate scaled bounding radius
    auto scale = moc.get_scale();
    float max_scale = glm::max(glm::max(glm::abs(scale.x), glm::abs(scale.y)), glm::abs(scale.z));
    float scaled_radius = mesh_data->m_bounding_radius * max_scale;

    if (!object_data)
    {
        object_data =
            glob::glob_state().getr_vulkan_render().get_cache().objects.alloc(moc.get_id());

        if (!glob::glob_state().getr_vulkan_render_loader().update_object(
                *object_data, *mat_data, *mesh_data, moc.get_transform_matrix(),
                moc.get_normal_matrix(), glm::vec3(moc.get_world_position())))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }

        object_data->gpu_data.bounding_radius = scaled_radius;
        moc.set_render_object_data(object_data);

        auto new_rqid = ::kryga::render_bridge::make_qid(*mat_data, *mesh_data);
        object_data->queue_id = new_rqid;
        glob::glob_state().getr_vulkan_render().schedule_to_drawing(object_data);
    }
    else
    {
        if (!glob::glob_state().getr_vulkan_render_loader().update_object(
                *object_data, *mat_data, *mesh_data, moc.get_transform_matrix(),
                moc.get_normal_matrix(), glm::vec3(moc.get_world_position())))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }

        object_data->gpu_data.bounding_radius = scaled_radius;

        auto new_rqid = ::kryga::render_bridge::make_qid(*mat_data, *mesh_data);
        auto& rqid = object_data->queue_id;
        if (new_rqid != rqid)
        {
            glob::glob_state().getr_vulkan_render().remove_from_drawing(object_data);
            object_data->queue_id = new_rqid;
            glob::glob_state().getr_vulkan_render().schedule_to_drawing(object_data);
        }
    }

    glob::glob_state().getr_vulkan_render().schedule_game_data_gpu_upload(object_data);

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
        KRG_return_nok(rc);
    }

    auto mesh_model = moc.get_mesh();

    if (mesh_model && is_same_source(*ctx.obj, *mesh_model))
    {
        rc = ctx.rb->render_dtor(*moc.get_mesh(), ctx.flag);
        KRG_return_nok(rc);
    }
    auto object_data = moc.get_render_object_data();

    if (object_data)
    {
        glob::glob_state().getr_vulkan_render().remove_from_drawing(object_data);
        glob::glob_state().getr_vulkan_render().get_cache().objects.release(object_data);
    }

    rc = root::game_object_component__render_destructor(ctx);
    KRG_return_nok(rc);

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
        rh = glob::glob_state().getr_vulkan_render().get_cache().directional_lights.alloc(
            lc_model.get_id());

        rh->gpu_data.ambient = lc_model.get_ambient();
        rh->gpu_data.diffuse = lc_model.get_diffuse();
        rh->gpu_data.specular = lc_model.get_specular();
        rh->gpu_data.direction = lc_model.get_direction();

        lc_model.set_handler(rh);
    }

    glob::glob_state().getr_vulkan_render().schedule_directional_light_data_gpu_upload(rh);

    return result_code::ok;
}
result_code
directional_light_component__render_destructor(reflection::type_context__render& ctx)
{
    auto& plc_model = ctx.obj->asr<base::directional_light_component>();
    if (auto h = plc_model.get_handler())
    {
        glob::glob_state().getr_vulkan_render().get_cache().directional_lights.release(h);
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
        rh = glob::glob_state().getr_vulkan_render().get_cache().universal_lights.alloc(
            lc_model.get_id(), render::light_type::spot);

        rh->gpu_data.position = lc_model.get_world_position();
        rh->gpu_data.ambient = lc_model.get_ambient();
        rh->gpu_data.diffuse = lc_model.get_diffuse();
        rh->gpu_data.specular = lc_model.get_specular();
        rh->gpu_data.radius = lc_model.get_radius();
        rh->gpu_data.direction = lc_model.get_direction();
        rh->gpu_data.cut_off = glm::cos(glm::radians(lc_model.get_cut_off()));
        rh->gpu_data.outer_cut_off = glm::cos(glm::radians(lc_model.get_outer_cut_off()));

        lc_model.set_handler(rh);
    }

    glob::glob_state().getr_vulkan_render().schedule_universal_light_data_gpu_upload(rh);

    return result_code::ok;
}

result_code
spot_light_component__render_destructor(reflection::type_context__render& ctx)
{
    auto& slc_model = ctx.obj->asr<base::spot_light_component>();

    if (auto h = slc_model.get_handler())
    {
        glob::glob_state().getr_vulkan_render().get_cache().universal_lights.release(h);
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
        rh = glob::glob_state().getr_vulkan_render().get_cache().universal_lights.alloc(
            lc_model.get_id(), render::light_type::point);

        rh->gpu_data.position = lc_model.get_world_position();
        rh->gpu_data.ambient = lc_model.get_ambient();
        rh->gpu_data.diffuse = lc_model.get_diffuse();
        rh->gpu_data.specular = lc_model.get_specular();
        rh->gpu_data.radius = lc_model.get_radius();

        lc_model.set_handler(rh);
    }

    glob::glob_state().getr_vulkan_render().schedule_universal_light_data_gpu_upload(rh);

    return result_code::ok;
}

result_code
point_light_component__render_destructor(reflection::type_context__render& ctx)
{
    auto& plc_model = ctx.obj->asr<base::point_light_component>();
    if (auto h = plc_model.get_handler())
    {
        glob::glob_state().getr_vulkan_render().get_cache().universal_lights.release(h);
    }

    return result_code::ok;
}

/*===============================*/

result_code
animated_mesh_component__render_loader(reflection::type_context__render& ctx)
{
    auto rc = root::game_object_component__render_loader(ctx);
    KRG_return_nok(rc);

    auto& amc = ctx.obj->asr<base::animated_mesh_component>();

    if (amc.get_gltf().size() == 0)
    {
        return result_code::failed;
    }

    // Derive stem and directory from the gltf path
    auto gltf_path = amc.get_gltf().get_file();
    auto path_str = gltf_path.str();

    std::string file_name_part, ext_part;
    gltf_path.parse_file_name_and_ext(file_name_part, ext_part);
    auto stem = file_name_part;

    auto dir_path = gltf_path.parent();

    auto skeleton_id = AID(stem);
    auto mesh_id = AID(stem + "_skinned_mesh");

    auto& anim_sys = glob::glob_state().getr_animation_system();
    auto& render_loader = glob::glob_state().getr_vulkan_render_loader();

    // Load if skeleton not yet registered
    if (!anim_sys.get_skeleton(skeleton_id))
    {
        // Find .ozz files directory: check next to glb first, then scan packages
        auto ozz_dir = dir_path;
        auto skel_test = (dir_path / (stem + "_skeleton.ozz"));
        if (!skel_test.exists())
        {
            // Search packages directory for {stem}_skeleton.ozz
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

        // 1. Load ozz skeleton
        auto skel_path = (ozz_dir / (stem + "_skeleton.ozz")).str();
        ozz::animation::Skeleton ozz_skeleton;
        if (!animation::ozz_loader::load_skeleton(skel_path, ozz_skeleton))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }

        // 2. Load ozz animations — scan for {stem}_*.ozz files (excluding skeleton)
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

            // Must start with "{stem}_" and end with ".ozz"
            if (fname.substr(0, stem.size() + 1) != stem + "_")
            {
                continue;
            }

            if (fname.size() < 4 || fname.substr(fname.size() - 4) != ".ozz")
            {
                continue;
            }

            // Skip the skeleton file
            if (fname.size() >= skel_suffix.size() &&
                fname.substr(fname.size() - skel_suffix.size()) == skel_suffix)
            {
                continue;
            }

            // Extract clip name: {stem}_{clip_name}.ozz
            auto clip_name = fname.substr(stem.size() + 1, fname.size() - stem.size() - 1 - 4);

            auto anim_path = (ozz_dir / fname).str();
            ozz::animation::Animation ozz_anim;
            if (animation::ozz_loader::load_animation(anim_path, ozz_anim))
            {
                anim_sys.register_animation(skeleton_id, AID(clip_name), std::move(ozz_anim));
            }
        }

        // 3. Load glTF for mesh vertices + inverse bind matrices + joint names
        animation::gltf_load_result gltf_result;
        if (!animation::gltf_animation_loader::load(amc.get_gltf(), gltf_result))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }

        // 4. Build joint remap table: mesh_bone_index -> ozz_skeleton_joint_index
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

        // 5. Register skeleton with inverse bind matrices and joint remaps
        anim_sys.register_skeleton(skeleton_id, std::move(ozz_skeleton),
                                   std::move(gltf_result.inverse_bind_matrices),
                                   std::move(joint_remaps));

        // 6. Create skinned mesh from the first mesh in the glTF
        if (!gltf_result.meshes.empty() && !render_loader.get_mesh_data(mesh_id))
        {
            auto& gltf_mesh = gltf_result.meshes[0];

            utils::buffer vert_buf(gltf_mesh.vertices.size() * sizeof(gpu::skinned_vertex_data));
            memcpy(vert_buf.data(), gltf_mesh.vertices.data(),
                   gltf_mesh.vertices.size() * sizeof(gpu::skinned_vertex_data));

            utils::buffer idx_buf(gltf_mesh.indices.size() * sizeof(gpu::uint));
            memcpy(idx_buf.data(), gltf_mesh.indices.data(),
                   gltf_mesh.indices.size() * sizeof(gpu::uint));

            auto vbv = vert_buf.make_view<gpu::skinned_vertex_data>();
            auto ibv = idx_buf.make_view<gpu::uint>();

            render_loader.create_skinned_mesh(mesh_id, vbv, ibv);
        }
    }

    auto* mesh_data = render_loader.get_mesh_data(mesh_id);
    if (!mesh_data)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    // Handle material
    auto* mat_model = amc.get_material();
    render::material_data* mat_data = nullptr;

    if (mat_model)
    {
        rc = ctx.rb->render_ctor(*mat_model, ctx.flag);
        KRG_return_nok(rc);
        mat_data = mat_model->get_material_data();
    }

    if (!mat_data)
    {
        ALOG_LAZY_ERROR;
        return result_code::failed;
    }

    auto* object_data = amc.get_render_object_data();

    amc.update_matrix();

    auto scale = amc.get_scale();
    float max_scale = glm::max(glm::max(glm::abs(scale.x), glm::abs(scale.y)), glm::abs(scale.z));
    float scaled_radius = mesh_data->m_bounding_radius * max_scale;

    auto* skel = anim_sys.get_skeleton(skeleton_id);
    uint32_t bone_count = skel ? static_cast<uint32_t>(skel->num_joints()) : 0;

    if (!object_data)
    {
        object_data =
            glob::glob_state().getr_vulkan_render().get_cache().objects.alloc(amc.get_id());

        if (!render_loader.update_object(*object_data, *mat_data, *mesh_data,
                                         amc.get_transform_matrix(), amc.get_normal_matrix(),
                                         glm::vec3(amc.get_world_position())))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }

        object_data->gpu_data.bounding_radius = scaled_radius;
        object_data->bone_count = bone_count;
        amc.set_render_object_data(object_data);

        auto new_rqid = ::kryga::render_bridge::make_qid(*mat_data, *mesh_data);
        object_data->queue_id = new_rqid;
        glob::glob_state().getr_vulkan_render().schedule_to_drawing(object_data);
    }
    else
    {
        if (!render_loader.update_object(*object_data, *mat_data, *mesh_data,
                                         amc.get_transform_matrix(), amc.get_normal_matrix(),
                                         glm::vec3(amc.get_world_position())))
        {
            ALOG_LAZY_ERROR;
            return result_code::failed;
        }

        object_data->gpu_data.bounding_radius = scaled_radius;
        object_data->bone_count = bone_count;

        auto new_rqid = ::kryga::render_bridge::make_qid(*mat_data, *mesh_data);
        auto& rqid = object_data->queue_id;
        if (new_rqid != rqid)
        {
            glob::glob_state().getr_vulkan_render().remove_from_drawing(object_data);
            object_data->queue_id = new_rqid;
            glob::glob_state().getr_vulkan_render().schedule_to_drawing(object_data);
        }
    }

    // Create animation instance
    auto clip_name = amc.get_clip_name();
    if (clip_name.valid() && anim_sys.has_animation(skeleton_id, clip_name))
    {
        auto inst_id = anim_sys.create_instance(amc.get_id(), skeleton_id, clip_name, object_data);
        amc.set_skeleton_id(skeleton_id);
        amc.set_anim_instance_id(inst_id);
    }

    glob::glob_state().getr_vulkan_render().schedule_game_data_gpu_upload(object_data);

    return result_code::ok;
}

result_code
animated_mesh_component__render_destructor(reflection::type_context__render& ctx)
{
    auto& amc = ctx.obj->asr<base::animated_mesh_component>();

    // Destroy animation instance
    auto& inst_id = amc.get_animation_instance_id();
    if (inst_id.valid())
    {
        glob::glob_state().getr_animation_system().destroy_instance(inst_id);
    }

    // Release material
    auto mat_model = amc.get_material();
    result_code rc = result_code::nav;
    if (mat_model && is_same_source(*ctx.obj, *mat_model))
    {
        rc = ctx.rb->render_dtor(*mat_model, ctx.flag);
        KRG_return_nok(rc);
    }

    // Release render object
    auto object_data = amc.get_render_object_data();
    if (object_data)
    {
        glob::glob_state().getr_vulkan_render().remove_from_drawing(object_data);
        glob::glob_state().getr_vulkan_render().get_cache().objects.release(object_data);
    }

    rc = root::game_object_component__render_destructor(ctx);
    KRG_return_nok(rc);

    return result_code::ok;
}

}  // namespace kryga
