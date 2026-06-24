#include "render_translator/render_command_processor.h"

#include "render_translator/render_commands.h"
#include "render_translator/render_convert.h"

#include <core/subsystem_queues.h>

#include <global_state/global_state.h>

#include <vulkan_render/render_system.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/types/vulkan_render_data.h>
#include <vulkan_render/types/vulkan_render_pass.h>
#include <vulkan_render/types/vulkan_material_data.h>
#include <vulkan_render/types/vulkan_texture_data.h>
#include <vulkan_render/types/vulkan_shader_effect_data.h>
#include <vulkan_render/types/vulkan_mesh_data.h>
#include <vulkan_render/types/vulkan_gpu_types.h>

#include <gpu_types/gpu_generic_constants.h>

#include <utils/buffer.h>
#include <utils/kryga_log.h>

#include <cstring>

// Render commands are PURE DATA (no methods). All execution lives here, alongside the
// processor that drains them — the render twin of audio_message_processor::apply and
// physics_command_processor::apply, which likewise hold their subsystem's per-message
// handling. Each behavior is a free process(cmd, ctx) overload; apply() switches on
// cmd_kind, runs the matching one, then destructs the command (the arena only rewinds;
// non-trivial members like shared_ptr / vector / dynobj are released by the ~T()).

namespace kryga
{

// ============================================================================
// Common — per-frame value updates
// ============================================================================

static void
process(update_transform_cmd& c, render_cmd::render_exec_context& ctx)
{
    auto* object_data = ctx.vr.get_cache().get_object(c.obj_handle);
    if (!object_data)
    {
        return;
    }

    object_data->gpu_data.model = c.transform;
    object_data->gpu_data.normal = c.normal_matrix;
    object_data->gpu_data.obj_pos = c.position;
    object_data->gpu_data.bounding_sphere_center = c.bounding_sphere_center;
    object_data->gpu_data.bounding_radius = c.bounding_radius;

    ctx.vr.stage_update_object(object_data);
}

static void
process(set_outline_cmd& c, render_cmd::render_exec_context& ctx)
{
    auto* object_data = ctx.vr.get_cache().get_object(c.obj_handle);
    if (!object_data)
    {
        // Object already destroyed (e.g. selection cleared during level unload),
        // or the selected component has no render object (null handle).
        return;
    }

    object_data->outlined = c.outlined;
    ctx.vr.stage_update_object_queue(object_data);
}

// ============================================================================
// Meshes / textures / shader effects / materials
// ============================================================================

static void
process(create_mesh_cmd& c, render_cmd::render_exec_context& ctx)
{
    // Populate the slot the builder pre-reserved. Handle-only — no id index.
    if (c.skinned)
    {
        auto vbv = c.vertices->make_view<gpu::skinned_vertex_data>();
        auto ibv = c.indices->make_view<gpu::uint>();
        ctx.loader.populate_skinned_mesh(c.handle, c.id, vbv, ibv);
    }
    else
    {
        auto vbv = c.vertices->make_view<gpu::vertex_data>();
        auto ibv = c.indices->make_view<gpu::uint>();
        ctx.loader.populate_mesh(c.handle, c.id, vbv, ibv);
    }
}

static void
process(destroy_mesh_cmd& c, render_cmd::render_exec_context& ctx)
{
    // [render thread] Release the data; the model already free()'d the slot.
    ctx.loader.reset_mesh_storage(c.handle);
}

static void
process(create_texture_cmd& c, render_cmd::render_exec_context& ctx)
{
    ctx.loader.populate_texture(c.handle, c.id, *c.pixels, c.width, c.height);
}

static void
process(destroy_texture_cmd& c, render_cmd::render_exec_context& ctx)
{
    ctx.loader.reset_texture_storage(c.handle);
}

static void
process(create_shader_effect_cmd& c, render_cmd::render_exec_context& ctx)
{
    auto* rp = ctx.vr.get_render_pass(AID("main"));
    auto se_data = rp->get_shader_effect(c.id);

    if (!se_data)
    {
        render::shader_effect_create_info se_ci;
        se_ci.vert_buffer = c.vert.get();
        se_ci.frag_buffer = c.frag.get();
        se_ci.is_vert_binary = c.is_vert_binary;
        se_ci.is_frag_binary = c.is_frag_binary;
        se_ci.is_wire = c.wire_topology;
        se_ci.alpha = c.enable_alpha ? render::alpha_mode::world : render::alpha_mode::none;
        se_ci.rp = rp;
        se_ci.enable_dynamic_state = false;
        se_ci.ds_mode = render::depth_stencil_mode::none;
        se_ci.cull_mode = se_ci.is_wire ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
        se_ci.spec_constants = c.spec_constants;

        rp->create_shader_effect(c.id, se_ci, se_data);
    }
}

static void
process(destroy_shader_effect_cmd& c, render_cmd::render_exec_context& ctx)
{
    auto* rp = ctx.vr.get_render_pass(AID("main"));
    auto* se_data = rp->get_shader_effect(c.id);
    if (se_data)
    {
        if (auto* owner_rp = se_data->get_owner_render_pass())
        {
            owner_rp->destroy_shader_effect(c.id);
        }
    }
}

static void
process(create_material_cmd& c, render_cmd::render_exec_context& ctx)
{
    auto* rp = ctx.vr.get_render_pass(AID("main"));
    auto* se_data = rp->get_shader_effect(c.shader_effect_id);

    if (!se_data || se_data->m_failed_load)
    {
        se_data = rp->get_shader_effect(AID("se_error"));
    }

    std::vector<render::texture_sampler_data> samples;
    for (auto& slot : c.texture_slots)
    {
        if (slot.texture_handle)
        {
            auto* td = ctx.loader.get_texture_data(slot.texture_handle);
            if (td)
            {
                render::texture_sampler_data tsd;
                tsd.texture = td;
                tsd.slot = slot.slot;
                samples.push_back(tsd);
            }
        }
    }

    uint32_t gpu_texture_indices[KGPU_MAX_TEXTURE_SLOTS];
    uint32_t gpu_sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
    for (int i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
    {
        gpu_texture_indices[i] = UINT32_MAX;
        gpu_sampler_indices[i] = 0;
    }

    for (auto& slot : c.texture_slots)
    {
        if (slot.texture_handle && slot.slot < KGPU_MAX_TEXTURE_SLOTS)
        {
            auto* td = ctx.loader.get_texture_data(slot.texture_handle);
            if (td)
            {
                gpu_texture_indices[slot.slot] = td->get_bindless_index();
            }
        }
        if (slot.slot < KGPU_MAX_TEXTURE_SLOTS)
        {
            gpu_sampler_indices[slot.slot] = slot.static_sampler_index;
        }
    }

    render_convert::set_material_texture_bindings(
        c.gpu_data, gpu_texture_indices, gpu_sampler_indices, KGPU_MAX_TEXTURE_SLOTS);

    // Handle path: populate the slot the builder pre-reserved, then read
    // back for the bindless-sampler setup below.
    ctx.loader.populate_material(c.handle, c.id, c.type_id, samples, *se_data, c.gpu_data);
    auto* mat_data = ctx.loader.get_material_data(c.handle);

    if (mat_data)
    {
        for (auto& slot : c.texture_slots)
        {
            if (slot.slot < KGPU_MAX_TEXTURE_SLOTS)
            {
                mat_data->set_bindless_sampler_index(slot.slot, slot.static_sampler_index);
            }
        }

        if (!c.gpu_data.empty())
        {
            ctx.vr.stage_add_material(mat_data);
        }
    }
}

static void
process(update_material_cmd& c, render_cmd::render_exec_context& ctx)
{
    if (!ctx.loader.material_valid(c.handle))
    {
        return;
    }
    auto* mat_data = ctx.loader.get_material_data(c.handle);
    if (!mat_data)
    {
        return;
    }

    auto* rp = ctx.vr.get_render_pass(AID("main"));
    auto* se_data = rp->get_shader_effect(c.shader_effect_id);

    if (!se_data || se_data->m_failed_load)
    {
        se_data = rp->get_shader_effect(AID("se_error"));
    }

    std::vector<render::texture_sampler_data> samples;
    for (auto& slot : c.texture_slots)
    {
        if (slot.texture_handle)
        {
            auto* td = ctx.loader.get_texture_data(slot.texture_handle);
            if (td)
            {
                render::texture_sampler_data tsd;
                tsd.texture = td;
                tsd.slot = slot.slot;
                samples.push_back(tsd);
            }
        }
    }

    uint32_t gpu_texture_indices[KGPU_MAX_TEXTURE_SLOTS];
    uint32_t gpu_sampler_indices[KGPU_MAX_TEXTURE_SLOTS];
    for (int i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
    {
        gpu_texture_indices[i] = UINT32_MAX;
        gpu_sampler_indices[i] = 0;
    }

    for (auto& slot : c.texture_slots)
    {
        if (slot.texture_handle && slot.slot < KGPU_MAX_TEXTURE_SLOTS)
        {
            auto* td = ctx.loader.get_texture_data(slot.texture_handle);
            if (td)
            {
                gpu_texture_indices[slot.slot] = td->get_bindless_index();
            }
        }
        if (slot.slot < KGPU_MAX_TEXTURE_SLOTS)
        {
            gpu_sampler_indices[slot.slot] = slot.static_sampler_index;
        }
    }

    render_convert::set_material_texture_bindings(
        c.gpu_data, gpu_texture_indices, gpu_sampler_indices, KGPU_MAX_TEXTURE_SLOTS);

    ctx.loader.update_material(*mat_data, samples, *se_data, c.gpu_data);

    for (auto& slot : c.texture_slots)
    {
        if (slot.slot < KGPU_MAX_TEXTURE_SLOTS)
        {
            mat_data->set_bindless_sampler_index(slot.slot, slot.static_sampler_index);
        }
    }

    if (!c.gpu_data.empty())
    {
        ctx.vr.stage_update_material(mat_data);
    }
}

static void
process(destroy_material_cmd& c, render_cmd::render_exec_context& ctx)
{
    if (!ctx.loader.material_valid(c.handle))
    {
        return;
    }
    auto* mat_data = ctx.loader.get_material_data(c.handle);
    if (mat_data)
    {
        ctx.vr.stage_remove_material(mat_data);
        // [render thread] Release the data; the model already free()'d the slot.
        ctx.loader.reset_material_storage(c.handle);
    }
}

// ============================================================================
// Objects — lightmap resolution helper (render thread, was packages/base anon ns)
// ============================================================================

namespace
{
struct resolved_lightmap
{
    glm::vec2 scale{1.0f, 1.0f};
    glm::vec2 offset{0.0f, 0.0f};
    uint32_t index = 0xFFFFFFFFu;
};

// Render-thread resolution of an instance's lightmap binding from the loader's
// per-level registry. Empty level_id (or a level/object without a baked entry)
// yields the unlit defaults. Runs at command execute, after create_lightmap_cmd
// has populated the registry for this level (same frame slot, earlier in FIFO).
resolved_lightmap
resolve_lightmap(render_cmd::render_exec_context& ctx,
                 const utils::id& level_id,
                 const utils::id& object_id)
{
    resolved_lightmap out;
    if (const auto* binding = ctx.loader.get_lightmap(level_id))
    {
        auto itr = binding->entries.find(object_id);
        if (itr != binding->entries.end())
        {
            out.scale = itr->second.scale;
            out.offset = itr->second.offset;
            out.index = binding->bindless_index;
        }
    }
    return out;
}
}  // namespace

static void
process(create_object_cmd& c, render_cmd::render_exec_context& ctx)
{
    auto* mesh_data = ctx.loader.get_mesh_data(c.mesh);
    auto* mat_data = ctx.loader.get_material_data(c.material);

    if (!mesh_data || !mat_data)
    {
        ALOG_ERROR("create_object: missing mesh or material for {}", c.id.str());
        return;
    }

    auto* object_data = ctx.vr.get_cache().create_object(c.obj_handle, c.id);

    if (!ctx.loader.update_object(
            *object_data, *mat_data, *mesh_data, c.transform, c.normal_matrix, c.position))
    {
        ALOG_LAZY_ERROR;
        return;
    }

    auto lm = resolve_lightmap(ctx, c.lightmap_level_id, c.id);
    object_data->gpu_data.bounding_radius = c.bounding_radius;
    object_data->gpu_data.bounding_sphere_center = c.bounding_sphere_center;
    object_data->gpu_data.lightmap_scale = lm.scale;
    object_data->gpu_data.lightmap_offset = lm.offset;
    object_data->gpu_data.lightmap_texture_index = lm.index;
    object_data->bone_count = c.bone_count;
    object_data->queue_id = std::move(c.queue_id);
    object_data->layer_flags = c.layer_flags.bits;

    ctx.vr.stage_add_object(object_data);
}

static void
process(update_object_cmd& c, render_cmd::render_exec_context& ctx)
{
    auto* object_data = ctx.vr.get_cache().get_object(c.obj_handle);
    if (!object_data)
    {
        return;
    }

    auto* mesh_data = ctx.loader.get_mesh_data(c.mesh);
    auto* mat_data = ctx.loader.get_material_data(c.material);

    if (!mesh_data || !mat_data)
    {
        return;
    }

    ctx.loader.update_object(
        *object_data, *mat_data, *mesh_data, c.transform, c.normal_matrix, c.position);

    auto lm = resolve_lightmap(ctx, c.lightmap_level_id, c.id);
    object_data->gpu_data.bounding_radius = c.bounding_radius;
    object_data->gpu_data.bounding_sphere_center = c.bounding_sphere_center;
    object_data->gpu_data.lightmap_scale = lm.scale;
    object_data->gpu_data.lightmap_offset = lm.offset;
    object_data->gpu_data.lightmap_texture_index = lm.index;

    auto new_rqid = std::move(c.queue_id);
    if (new_rqid != object_data->queue_id || c.layer_flags.bits != object_data->layer_flags)
    {
        ctx.vr.stage_remove_object(object_data);
        object_data->queue_id = std::move(new_rqid);
        object_data->layer_flags = c.layer_flags.bits;
        ctx.vr.stage_add_object(object_data);
    }
    else
    {
        ctx.vr.stage_update_object(object_data);
    }
}

static void
process(destroy_object_cmd& c, render_cmd::render_exec_context& ctx)
{
    auto* object_data = ctx.vr.get_cache().get_object(c.obj_handle);
    if (object_data)
    {
        ctx.vr.stage_remove_object(object_data);
        object_data->mark_pending_release();

        // Retire the slot: the handle reads stale immediately, but the payload
        // stays put. In-flight frames still hold raw pointers to it in their
        // upload queues, and the model-owned allocator's deferred free keeps
        // this slot index out of circulation until those frames drain — so the
        // data is only overwritten by a future create_object, never freed out
        // from under the GPU. No deferred delete needed.
        ctx.vr.get_cache().retire_object(c.obj_handle);
    }
}

static void
process(create_chunk_mesh_cmd& c, render_cmd::render_exec_context& ctx)
{
    auto vbv = c.vertices->make_view<gpu::vertex_data>();
    auto ibv = c.indices->make_view<gpu::uint>();
    ctx.loader.populate_mesh(c.handle, c.id, vbv, ibv);
}

// ============================================================================
// Lights
// ============================================================================

static void
process(create_light_cmd& c, render_cmd::render_exec_context& ctx)
{
    if (c.kind == light_kind::directional)
    {
        auto* rh = ctx.vr.get_cache().populate_dir_light(c.dir_handle, c.id);
        rh->gpu_data.ambient = c.ambient;
        rh->gpu_data.diffuse = c.diffuse;
        rh->gpu_data.specular = c.specular;
        rh->gpu_data.direction = c.direction;
        ctx.vr.stage_add_light(rh);
    }
    else
    {
        auto lt =
            (c.kind == light_kind::spot) ? render::light_type::spot : render::light_type::point;
        auto* rh = ctx.vr.get_cache().populate_uni_light(c.uni_handle, c.id, lt);
        rh->gpu_data.position = c.position;
        rh->gpu_data.ambient = c.ambient;
        rh->gpu_data.diffuse = c.diffuse;
        rh->gpu_data.specular = c.specular;
        rh->gpu_data.radius = c.radius;
        rh->gpu_data.direction = c.direction;
        rh->gpu_data.cut_off = c.cut_off;
        rh->gpu_data.outer_cut_off = c.outer_cut_off;
        ctx.vr.stage_add_light(rh);
    }
}

static void
process(update_light_cmd& c, render_cmd::render_exec_context& ctx)
{
    if (c.kind == light_kind::directional)
    {
        auto* rh = ctx.vr.get_cache().get_dir_light(c.dir_handle);
        if (!rh)
        {
            return;
        }

        rh->gpu_data.ambient = c.ambient;
        rh->gpu_data.diffuse = c.diffuse;
        rh->gpu_data.specular = c.specular;
        rh->gpu_data.direction = c.direction;
        ctx.vr.stage_update_light(rh);
    }
    else
    {
        auto* rh = ctx.vr.get_cache().get_uni_light(c.uni_handle);
        if (!rh)
        {
            return;
        }

        rh->gpu_data.position = c.position;
        rh->gpu_data.ambient = c.ambient;
        rh->gpu_data.diffuse = c.diffuse;
        rh->gpu_data.specular = c.specular;
        rh->gpu_data.radius = c.radius;
        rh->gpu_data.direction = c.direction;
        rh->gpu_data.cut_off = c.cut_off;
        rh->gpu_data.outer_cut_off = c.outer_cut_off;
        ctx.vr.stage_update_light(rh);
    }
}

static void
process(select_directional_light_cmd& c, render_cmd::render_exec_context& ctx)
{
    ctx.vr.set_selected_directional_light(c.handle);
}

static void
process(destroy_light_cmd& c, render_cmd::render_exec_context& ctx)
{
    if (c.kind == light_kind::directional)
    {
        auto* rh = ctx.vr.get_cache().get_dir_light(c.dir_handle);
        if (rh)
        {
            // Drop from the pending upload queues BEFORE releasing the slot,
            // else the per-frame queues keep a dangling pointer that the
            // render thread dereferences next draw (out-of-bounds SSBO write).
            ctx.vr.stage_remove_light(rh);
            ctx.vr.get_cache().retire_dir_light(c.dir_handle);
        }
    }
    else
    {
        auto* rh = ctx.vr.get_cache().get_uni_light(c.uni_handle);
        if (rh)
        {
            ctx.vr.stage_remove_light(rh);
            ctx.vr.get_cache().retire_uni_light(c.uni_handle);
        }
    }
}

static void
process(create_skinned_mesh_cmd& c, render_cmd::render_exec_context& ctx)
{
    auto vbv = c.vertices->make_view<gpu::skinned_vertex_data>();
    auto ibv = c.indices->make_view<gpu::uint>();
    ctx.loader.populate_skinned_mesh(c.handle, c.id, vbv, ibv);
}

// ============================================================================
// Lightmap
// ============================================================================

static void
process(create_lightmap_cmd& c, render_cmd::render_exec_context& ctx)
{
    utils::buffer buf(c.pixels.size());
    std::memcpy(buf.data(), c.pixels.data(), c.pixels.size());

    // The binding owns the atlas texture. Re-bake while loaded: update the
    // existing texture in place (same bindless slot, objects keep their
    // index). Fresh load: create it. Texture lifecycle goes through the
    // renderer (it owns the bindless slot identity).
    auto& renderer = glob::glob_state().getr_render().renderer;
    const auto* binding = ctx.loader.get_lightmap(c.level_id);
    auto* tex = binding ? binding->texture : nullptr;
    if (tex)
    {
        renderer.update_texture(tex,
                                buf,
                                c.width,
                                c.height,
                                VK_FORMAT_R16G16B16A16_SFLOAT,
                                render::texture_format::rgba16f);
    }
    else
    {
        tex = renderer.create_texture(c.tex_id,
                                      buf,
                                      c.width,
                                      c.height,
                                      VK_FORMAT_R16G16B16A16_SFLOAT,
                                      render::texture_format::rgba16f);
    }
    if (tex)
    {
        ctx.loader.set_lightmap(c.level_id, tex, std::move(c.entries));
    }
}

static void
process(destroy_lightmap_cmd& c, render_cmd::render_exec_context& ctx)
{
    ctx.loader.remove_lightmap(c.level_id);
}

// ============================================================================
// Bones
// ============================================================================

static void
process(apply_bones_cmd& c, render_cmd::render_exec_context& ctx)
{
    ctx.vr.get_bone_matrices_staging() = std::move(c.matrices);
    for (auto& u : c.updates)
    {
        u.rd->bone_offset = u.offset;
        u.rd->bone_count = u.count;
        u.rd->gpu_data.bone_offset = u.offset;
        u.rd->gpu_data.bone_count = u.count;
        ctx.vr.stage_update_object(u.rd);
    }
}

// ============================================================================
// UI panels (packages/ui retained-mode widgets)
// ============================================================================

namespace
{

// Pixel rect (top-left origin) -> NDC rect (min, max) using the current viewport
// size. Y maps directly: pixel +y = down, and Vulkan NDC +y also points down.
glm::vec4
ui_pixels_to_ndc(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t vw, uint32_t vh)
{
    float fvw = static_cast<float>(vw == 0 ? 1u : vw);
    float fvh = static_cast<float>(vh == 0 ? 1u : vh);

    float x0 = (static_cast<float>(x) / fvw) * 2.f - 1.f;
    float x1 = (static_cast<float>(x + w) / fvw) * 2.f - 1.f;
    float y0 = (static_cast<float>(y) / fvh) * 2.f - 1.f;
    float y1 = (static_cast<float>(y + h) / fvh) * 2.f - 1.f;

    return glm::vec4(x0, y0, x1, y1);
}

}  // namespace

static void
process(ui_panel_upsert_cmd& c, render_cmd::render_exec_context& ctx)
{
    if (!c.visible)
    {
        ctx.vr.ui_panel_destroy(c.id);
        return;
    }

    render::vulkan_render::ui_panel_entry entry;
    entry.rect_ndc = ui_pixels_to_ndc(c.x, c.y, c.w, c.h, ctx.vr.get_width(), ctx.vr.get_height());
    entry.color_opacity = glm::vec4(c.color, c.opacity);

    ctx.vr.ui_panel_create_or_update(c.id, entry);
}

static void
process(ui_panel_destroy_cmd& c, render_cmd::render_exec_context& ctx)
{
    ctx.vr.ui_panel_destroy(c.id);
}

static void
process(ui_text_upsert_cmd& c, render_cmd::render_exec_context& ctx)
{
    // Hidden / empty just clears the slot — the handle stays reserved (the widget
    // still exists); only the destroyer frees it.
    if (!c.visible || c.text[0] == '\0')
    {
        ctx.loader.reset_ui_text(c.handle);
        return;
    }

    render::ui_text_entry entry;
    entry.text = c.text;  // char[] -> std::string (null-terminated)
    entry.x = c.x;
    entry.y = c.y;
    entry.anchor = c.anchor;
    entry.font_size = c.font_size;
    entry.color = c.color;
    entry.font = c.font;

    ctx.loader.populate_ui_text(c.handle, entry);
}

static void
process(ui_text_destroy_cmd& c, render_cmd::render_exec_context& ctx)
{
    ctx.loader.reset_ui_text(c.handle);
}

// ============================================================================
// run_and_destroy — run the matching process() overload (ADL), then destruct the
// command in place (the arena only rewinds; ~T() releases non-trivial members).
// ============================================================================

namespace render_cmd
{
template <class T>
static void
run_and_destroy(render_command_base* base, render_exec_context& ctx)
{
    auto* c = static_cast<T*>(base);
    process(*c, ctx);  // ADL → kryga::process(T&, ...)
    c->~T();
}
}  // namespace render_cmd

void
render_command_processor::process(float /*dt*/, uint32_t frame)
{
    const uint32_t frame_slot = frame;

    // Drain this frame slot's queue to empty. All the frame's commands were pushed
    // (and made visible via the submitted-counter mutex handoff) before the render
    // thread was released, and the producer is on the other frame slot, so "empty"
    // reliably means "whole frame consumed".
    glob::glob_state()
        .getr_subsystem_queues()
        .render.queue(frame_slot)
        .drain([this](render_cmd::render_command_base*&& cmd) { apply(cmd); });
}

void
render_command_processor::apply(render_cmd::render_command_base* cmd)
{
    // Central tagged dispatch: switch on cmd_kind, run the matching process(), then
    // destruct. The exec context is just the bound renderer/loader refs, so minting it
    // per command is free.
    render_cmd::render_exec_context ctx{m_vr, m_loader};

    switch (cmd->cmd_kind)
    {
    case render_cmd::render_cmd_kind::update_transform:
        render_cmd::run_and_destroy<update_transform_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::set_outline:
        render_cmd::run_and_destroy<set_outline_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::create_mesh:
        render_cmd::run_and_destroy<create_mesh_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::destroy_mesh:
        render_cmd::run_and_destroy<destroy_mesh_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::create_texture:
        render_cmd::run_and_destroy<create_texture_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::destroy_texture:
        render_cmd::run_and_destroy<destroy_texture_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::create_shader_effect:
        render_cmd::run_and_destroy<create_shader_effect_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::destroy_shader_effect:
        render_cmd::run_and_destroy<destroy_shader_effect_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::create_material:
        render_cmd::run_and_destroy<create_material_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::update_material:
        render_cmd::run_and_destroy<update_material_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::destroy_material:
        render_cmd::run_and_destroy<destroy_material_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::create_object:
        render_cmd::run_and_destroy<create_object_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::update_object:
        render_cmd::run_and_destroy<update_object_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::destroy_object:
        render_cmd::run_and_destroy<destroy_object_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::create_chunk_mesh:
        render_cmd::run_and_destroy<create_chunk_mesh_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::create_light:
        render_cmd::run_and_destroy<create_light_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::update_light:
        render_cmd::run_and_destroy<update_light_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::select_directional_light:
        render_cmd::run_and_destroy<select_directional_light_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::destroy_light:
        render_cmd::run_and_destroy<destroy_light_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::create_skinned_mesh:
        render_cmd::run_and_destroy<create_skinned_mesh_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::create_lightmap:
        render_cmd::run_and_destroy<create_lightmap_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::destroy_lightmap:
        render_cmd::run_and_destroy<destroy_lightmap_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::apply_bones:
        render_cmd::run_and_destroy<apply_bones_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::ui_panel_upsert:
        render_cmd::run_and_destroy<ui_panel_upsert_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::ui_panel_destroy:
        render_cmd::run_and_destroy<ui_panel_destroy_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::ui_text_upsert:
        render_cmd::run_and_destroy<ui_text_upsert_cmd>(cmd, ctx);
        break;
    case render_cmd::render_cmd_kind::ui_text_destroy:
        render_cmd::run_and_destroy<ui_text_destroy_cmd>(cmd, ctx);
        break;
    }
}

}  // namespace kryga
