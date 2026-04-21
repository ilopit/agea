#include "packages/ui/package.ui.h"

#include <global_state/global_state.h>
#include <render_bridge/render_bridge.h>
#include <render_bridge/render_command.h>
#include <render_bridge/render_commands_common.h>

#include <core/object_layer_flags.h>

#include "packages/root/model/assets/mesh.h"
#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/shader_effect.h"
#include "packages/ui/render/overrides/render_types_handlers.h"
#include "packages/ui/model/components/ui_panel.h"

#include <vulkan_render/types/vulkan_mesh_data.h>
#include <vulkan_render/types/vulkan_material_data.h>
#include <vulkan_render/types/vulkan_gpu_types.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/kryga_render.h>

#include <utils/kryga_log.h>

namespace kryga
{

namespace
{

// The UI panel rides the existing render-object pipeline: a fixed plane_mesh
// (system mesh, NDC [-1,1] quad) + a material whose shader_effect outputs NDC
// directly from the model transform. To stay self-contained we replicate the
// minimal create/update/destroy commands here — mesh and material are looked
// up by id at execute time, same as the base package pattern.

struct ui_create_object_cmd : render_cmd::render_command_base
{
    utils::id id;
    utils::id mesh_id;
    utils::id material_id;
    glm::mat4 transform{1.0f};
    glm::mat4 normal_matrix{1.0f};
    glm::vec3 position{0.0f};
    std::string queue_id;
    core::object_layer_flags layer_flags;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        auto* mesh_data = ctx.loader.get_mesh_data(mesh_id);
        auto* mat_data = ctx.loader.get_material_data(material_id);

        if (!mesh_data || !mat_data)
        {
            ALOG_ERROR("ui_create_object: missing mesh or material for {}", id.str());
            return;
        }

        auto* object_data = ctx.vr.get_cache().objects.alloc(id);

        if (!ctx.loader.update_object(
                *object_data, *mat_data, *mesh_data, transform, normal_matrix, position))
        {
            ALOG_LAZY_ERROR;
            return;
        }

        // Large bounding radius keeps the panel from being frustum-culled —
        // its vertex shader discards camera projection entirely and emits NDC
        // directly, so world-space culling would be wrong anyway.
        object_data->gpu_data.bounding_radius = 1.0e6f;
        object_data->bone_count = 0;
        object_data->queue_id = std::move(queue_id);
        object_data->layer_flags = layer_flags.bits;

        ctx.vr.schd_add_object(object_data);
    }
};

struct ui_update_object_cmd : render_cmd::render_command_base
{
    utils::id id;
    utils::id mesh_id;
    utils::id material_id;
    glm::mat4 transform{1.0f};
    glm::mat4 normal_matrix{1.0f};
    glm::vec3 position{0.0f};
    std::string queue_id;
    core::object_layer_flags layer_flags;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        auto* object_data = ctx.vr.get_cache().objects.find_by_id(id);
        if (!object_data)
        {
            return;
        }

        auto* mesh_data = ctx.loader.get_mesh_data(mesh_id);
        auto* mat_data = ctx.loader.get_material_data(material_id);

        if (!mesh_data || !mat_data)
        {
            return;
        }

        ctx.loader.update_object(
            *object_data, *mat_data, *mesh_data, transform, normal_matrix, position);

        object_data->gpu_data.bounding_radius = 1.0e6f;

        auto new_rqid = std::move(queue_id);
        if (new_rqid != object_data->queue_id ||
            layer_flags.bits != object_data->layer_flags)
        {
            ctx.vr.schd_remove_object(object_data);
            object_data->queue_id = std::move(new_rqid);
            object_data->layer_flags = layer_flags.bits;
            ctx.vr.schd_add_object(object_data);
        }
        else
        {
            ctx.vr.schd_update_object(object_data);
        }
    }
};

struct ui_destroy_object_cmd : render_cmd::render_command_base
{
    utils::id id;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        auto* object_data = ctx.vr.get_cache().objects.find_by_id(id);
        if (object_data)
        {
            ctx.vr.schd_remove_object(object_data);
            ctx.vr.get_cache().objects.release(object_data);
        }
    }
};

}  // namespace

result_code
ui_panel__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto rc = root::game_object_component__cmd_builder(ctx);
    KRG_return_nok(rc);

    auto& panel = ctx.obj->asr<ui::ui_panel>();

    if (!panel.get_material())
    {
        return result_code::failed;
    }

    rc = ctx.rb->render_cmd_build(*panel.get_material(), ctx.flag);
    KRG_return_nok(rc);

    panel.update_matrix();

    const utils::id mesh_id = AID("plane_mesh");
    const auto material_id = panel.get_material()->get_id();

    // Panels ride the transparent queue so they draw after opaque geometry
    // and alpha-blend cleanly. The shader forces gl_Position.z to a near-front
    // depth, so final draw order among panels is queue order, not depth.
    std::string new_rqid = "transparent";

    if (!panel.get_render_built())
    {
        auto* cmd = ctx.rb->alloc_cmd<ui_create_object_cmd>();
        cmd->id = panel.get_id();
        cmd->mesh_id = mesh_id;
        cmd->material_id = material_id;
        cmd->transform = panel.get_transform_matrix();
        cmd->normal_matrix = panel.get_normal_matrix();
        cmd->position = glm::vec3(panel.get_world_position());
        cmd->queue_id = new_rqid;
        cmd->layer_flags = panel.get_layers();

        panel.set_render_built(true);
        ctx.rb->enqueue_cmd(cmd);
    }
    else
    {
        auto* cmd = ctx.rb->alloc_cmd<ui_update_object_cmd>();
        cmd->id = panel.get_id();
        cmd->mesh_id = mesh_id;
        cmd->material_id = material_id;
        cmd->transform = panel.get_transform_matrix();
        cmd->normal_matrix = panel.get_normal_matrix();
        cmd->position = glm::vec3(panel.get_world_position());
        cmd->queue_id = new_rqid;
        cmd->layer_flags = panel.get_layers();

        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

result_code
ui_panel__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& panel = ctx.obj->asr<ui::ui_panel>();

    auto* mat_model = panel.get_material();

    result_code rc = result_code::nav;
    if (mat_model && (panel.get_package() == mat_model->get_package() ||
                       panel.get_level() == mat_model->get_level()))
    {
        rc = ctx.rb->render_cmd_destroy(*mat_model, ctx.flag);
        KRG_return_nok(rc);
    }

    if (panel.get_render_built())
    {
        auto* cmd = ctx.rb->alloc_cmd<ui_destroy_object_cmd>();
        cmd->id = panel.get_id();
        panel.set_render_built(false);
        ctx.rb->enqueue_cmd(cmd);
    }

    rc = root::game_object_component__cmd_destroyer(ctx);
    KRG_return_nok(rc);

    return result_code::ok;
}

}  // namespace kryga
