#pragma once

#include "vulkan_render/types/vulkan_render_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/types/vulkan_light_data.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/types/vulkan_compute_shader_data.h"
#include "vulkan_render/utils/vulkan_buffer.h"
#include "vulkan_render/utils/vulkan_image.h"
#include "vulkan_render/utils/segments.h"
#include "vulkan_render/types/vulkan_render_pass.h"
#include "vulkan_render/vulkan_render_graph.h"
#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/render_enums.h"
#include "vulkan_render/render_config.h"
#include "render/utils/frustum.h"
#include "render/utils/cluster_grid.h"
#include "render/utils/object_bvh.h"
#include "gpu_types/gpu_cluster_types.h"
#include "gpu_types/gpu_shadow_types.h"
#include "gpu_types/gpu_probe_types.h"

#include <utils/buffer.h>
#include <utils/check.h>
#include <utils/dynamic_object.h>
#include <utils/id.h>
#include <utils/line_container.h>
#include <utils/id_allocator.h>
#include <render_types/render_handle.h>

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <mutex>
#include <vector>

namespace kryga
{
namespace render
{

class vulkan_render_loader;
class render_device;
struct frame_data;
class shader_effect_data;

using render_line_container = ::kryga::utils::line_container<render::vulkan_render_data*>;

using materials_update_queue = ::kryga::utils::line_container<render::material_data*>;
using materials_update_queue_set = ::kryga::utils::line_container<materials_update_queue>;
using objects_update_queue = ::kryga::utils::line_container<render::vulkan_render_data*>;
using textures_update_queue = ::kryga::utils::line_container<render::texture_data*>;

using directional_light_update_queue =
    ::kryga::utils::line_container<render::vulkan_directional_light_data*>;
using universal_light_update_queue =
    ::kryga::utils::line_container<render::vulkan_universal_light_data*>;

struct pipeline_ctx
{
    uint32_t cur_material_type_idx = INVALID_GPU_INDEX;
    uint32_t cur_material_idx = INVALID_GPU_INDEX;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
};

struct frame_buffers
{
    // Dynamic uniform buffer
    vk_utils::vulkan_buffer dynamic_data;

    // SSBOs
    vk_utils::vulkan_buffer objects;
    vk_utils::vulkan_buffer universal_lights;
    vk_utils::vulkan_buffer directional_lights;
    vk_utils::vulkan_buffer materials;

    // Cluster lighting SSBOs
    vk_utils::vulkan_buffer cluster_counts;
    vk_utils::vulkan_buffer cluster_indices;
    vk_utils::vulkan_buffer cluster_config;

    // Instance slots buffer for instanced drawing
    vk_utils::vulkan_buffer instance_slots;

    // Bone matrices SSBO for skeletal animation
    vk_utils::vulkan_buffer bone_matrices;

    // GPU frustum culling buffers
    vk_utils::vulkan_buffer frustum_data;     // Frustum planes (uniform)
    vk_utils::vulkan_buffer visible_indices;  // Output visible object indices
    vk_utils::vulkan_buffer cull_output;      // Visible count + indirect commands

    // Shadow data SSBO
    vk_utils::vulkan_buffer shadow_data;

    // Light probe SSBOs
    vk_utils::vulkan_buffer probe_data;
    vk_utils::vulkan_buffer probe_grid;
};

struct frame_upload_state
{
    objects_update_queue objects_queue;
    materials_update_queue_set materials_queue_set;
    directional_light_update_queue directional_light_queue;
    universal_light_update_queue universal_light_queue;
    textures_update_queue textures_queue;

    bool has_pending_materials = false;

    bool
    has_objects() const
    {
        return !objects_queue.empty();
    }

    bool
    has_universal_lights() const
    {
        return !universal_light_queue.empty();
    }

    bool
    has_directional_lights() const
    {
        return !directional_light_queue.empty();
    }

    bool
    has_materials() const
    {
        return has_pending_materials;
    }

    bool
    has_textures() const
    {
        return !textures_queue.empty();
    }

    void
    clear_all()
    {
        objects_queue.clear();
        universal_light_queue.clear();
        directional_light_queue.clear();
        textures_queue.clear();

        for (auto& m : materials_queue_set)
        {
            m.clear();
        }

        has_pending_materials = false;
    }
};

struct ui_frame_state
{
    vk_utils::vulkan_buffer vertex_buffer;
    vk_utils::vulkan_buffer index_buffer;

    int32_t vertex_count = 0;
    int32_t index_count = 0;
};

// One ImGui draw command flattened for the render thread. Plain data so the
// public header stays free of <imgui.h>. Offsets are precomputed to match how
// update_ui concatenates per-cmd-list vertices/indices into a single buffer.
struct ui_draw_cmd
{
    float clip[4] = {0.f, 0.f, 0.f, 0.f};  // x0, y0, x1, y1 in framebuffer pixels
    uint32_t elem_count = 0;
    uint32_t idx_offset = 0;  // running first-index across all commands
    int32_t vtx_offset = 0;   // base vertex of this command's cmd-list
};

// Owned, double-buffered snapshot of one ImGui frame's draw data. The main
// thread fills a back buffer in capture_ui_snapshot() and publishes it; the
// render thread reads the published one. This decouples rendering from the
// live (single-buffered) ImGui draw data, so the main thread's next
// ImGui::NewFrame() can never stomp data the render thread is mid-read — the
// race the "wait before ui_tick" ordering currently works around.
struct ui_draw_snapshot
{
    bool valid = false;
    float display_size[2] = {0.f, 0.f};
    uint32_t total_vtx = 0;
    uint32_t total_idx = 0;
    std::vector<uint8_t> vtx;  // concatenated ImDrawVert bytes
    std::vector<uint8_t> idx;  // concatenated ImDrawIdx bytes
    std::vector<ui_draw_cmd> cmds;
};

struct frame_state
{
    frame_buffers buffers;
    frame_upload_state uploads;
    ui_frame_state ui;

    frame_data* frame = nullptr;
};

struct draw_batch
{
    mesh_data* mesh;
    material_data* material;
    uint32_t instance_count;
    uint32_t first_instance_offset;  // offset into instance_slots buffer
    bool outlined;
    bool cast_shadows;
};

class vulkan_render
{
public:
    vulkan_render();
    ~vulkan_render();

    void
    init(uint32_t w, uint32_t h, const render_config& config, bool only_rp = false);

    void
    deinit();

    // Live render-scale change. Rebuilds scene_lowres images + main-pass
    // framebuffer/depth at the new size; keeps VkRenderPass + all shader
    // effects intact (compatible since attachment formats don't change and
    // pipelines use dynamic viewport). Returns false if render_scale isn't
    // enabled or the divisor is invalid.
    bool
    reconfigure_render_scale_live(uint32_t new_divisor);

    // Toggle render_scale on/off at runtime. Rebuilds the composite pass +
    // scene_upscale / depth_outline / ui_copy shader effects, swaps main
    // pass color targets between scene_lowres images and the swapchain
    // (preserving main pass's VkRenderPass identity so other SEs survive),
    // and rebuilds the render graph. Materials, textures, meshes, frames,
    // bindless texture indices, and game-object render data all survive.
    bool
    reconfigure_render_scale_enabled(bool enabled);

    // Recreate the swapchain at runtime with `count` images and present `mode`.
    // Both frames_in_flight and present mode route through here since each is a
    // swapchain-recreate trigger; the engine keeps frames_in_flight == swapchain
    // image count, so this rebuilds the swapchain-backed pass framebuffers and
    // frees/creates the per-frame GPU buffer sets to match — reclaiming memory
    // when the count is lowered. mailbox forces count up to 3.
    void
    reconfigure_swapchain(uint32_t count, present_mode mode);

    void
    set_camera(gpu::camera_data d);

    // Returns the camera the main thread last wrote for the frame it is
    // building (its current input slot), so a main-thread query reflects the
    // latest set value even before the render thread has latched it.
    const gpu::camera_data&
    get_camera() const
    {
        return m_camera_pending[m_build_frame_slot];
    }

    // Main thread: select the per-frame double-buffer frame slot (frame parity)
    // that subsequent set_camera() / capture_ui_snapshot() calls write into. Set
    // once per frame (via the pipeline's begin_frame). Pairs with
    // set_draw_frame_slot, the render thread's read-side selector. With the
    // depth-1 pipeline gate the two never name the same frame slot at once.
    void
    set_build_frame_slot(uint32_t frame_slot)
    {
        m_build_frame_slot = frame_slot & 1u;
    }

    // Render thread: select the frame slot draw_main()/apply_pending_camera()/
    // update_ui()/draw_ui() read this frame. The render loop stamps it (= the
    // frame's parity) before draw_main. Left at 0 for synchronous
    // (test/headless) draws that don't go through the streaming loop.
    void
    set_draw_frame_slot(uint32_t frame_slot)
    {
        m_draw_frame_slot = frame_slot & 1u;
    }

    // Snapshot the current ImGui frame's draw data into the main thread's
    // current input slot for the render thread. Called on the MAIN thread right
    // after ImGui::Render(), before the next NewFrame(). No-op without an ImGui
    // context. Decouples the render thread from the live ImGui draw data.
    void
    capture_ui_snapshot();

    uint32_t
    width() const
    {
        return m_width;
    }

    uint32_t
    height() const
    {
        return m_height;
    }

    void
    draw_main();

    void
    draw_headless();

    // clang-format off
    void stage_add_object(render::vulkan_render_data* obj_data);
    void stage_add_material(render::material_data* mat_data);
    void stage_add_light(render::vulkan_directional_light_data* ld);
    void stage_add_light(render::vulkan_universal_light_data* ld);

    void stage_update_object(render::vulkan_render_data* obj_data);
    void stage_update_object_queue(render::vulkan_render_data* obj_data);
    void stage_update_material(render::material_data* mat_data);
    void stage_update_light(render::vulkan_directional_light_data* ld);
    void stage_update_light(render::vulkan_universal_light_data* ld);
    void stage_update_texture(render::texture_data* tex);
    void flush_pending_texture_updates(); // main thread only; for immediate-submit paths

    void stage_remove_object(render::vulkan_render_data* obj_data);
    void stage_remove_material(render::material_data* mat_data);
    // Drop a light from every frame's pending upload queue. MUST be called when
    // a light's render data is released, else the per-frame queues keep a
    // dangling pointer that upload_gpu_*_light_data dereferences next draw.
    void stage_remove_light(render::vulkan_directional_light_data* ld);
    void stage_remove_light(render::vulkan_universal_light_data* ld);
    // clang-format on

    void
    set_selected_directional_light(render::types::directional_light_handle h);

    uint32_t
    get_selected_directional_light_slot();

    std::vector<glm::mat4>&
    get_bone_matrices_staging()
    {
        return m_bone_matrices_staging;
    }

    // Replace the current set of light probes (SH L2 coefficients indexed by
    // gpu::object_data::probe_index). Buffered like other stage_* APIs: the
    // payload is stashed on the cache and applied to each frame's SSBO at
    // its next prepare_draw_resources, so callers don't need to know the
    // current frame index and don't risk writing to a frame that's still
    // in-flight on the GPU.
    void
    stage_set_probes(std::vector<gpu::sh_probe> probes, const gpu::probe_grid_config& grid_config);

    void
    clear_upload_queue();

    vulkan_render_data*
    object_id_under_coordinate(uint32_t x, uint32_t y);

    // The merged render-resource owner (render_system.loader). render_cache was
    // absorbed into vulkan_render_loader; this accessor keeps the old name for
    // object/light/bindless-texture call sites. Bound at init().
    vulkan_render_loader&
    get_cache();

    // --- System / bindless resource lifecycle ------------------------------
    // The renderer owns slot identity for render-created resources (system
    // meshes/materials, bindless textures): reserve/reclaim happen HERE; the
    // loader only builds + stores at the reserved slot. Mirrors the content
    // split, where render_translator owns identity and the loader owns storage.
    mesh_data*
    create_mesh(const utils::id& mesh_id,
                utils::buffer_view<gpu::vertex_data> vertices,
                utils::buffer_view<gpu::uint> indices);
    mesh_data*
    create_skinned_mesh(const utils::id& mesh_id,
                        utils::buffer_view<gpu::skinned_vertex_data> vertices,
                        utils::buffer_view<gpu::uint> indices);
    mesh_data*
    get_system_mesh_data(render::types::mesh_handle h);
    bool
    system_mesh_valid(render::types::mesh_handle h) const;
    void
    destroy_system_mesh_data(render::types::mesh_handle h);

    material_data*
    create_material(const utils::id& id,
                    const utils::id& type_id,
                    std::vector<texture_sampler_data>& textures_data,
                    shader_effect_data& se_data,
                    const utils::dynobj& params);
    material_data*
    get_system_material_data(render::types::material_handle h);
    bool
    system_material_valid(render::types::material_handle h) const;
    void
    destroy_system_material_data(render::types::material_handle h);

    // Bindless textures: reserve a slot (slot index == bindless index) and hand
    // back an initialized texture_data; release returns the slot to the pool.
    texture_data*
    alloc_texture(const utils::id& id);
    void
    release_texture(texture_data* td);

    texture_data*
    create_texture(const utils::id& texture_id,
                   const utils::buffer& base_color,
                   uint32_t w,
                   uint32_t h);
    texture_data*
    create_texture(const utils::id& texture_id,
                   const utils::buffer& data,
                   uint32_t w,
                   uint32_t h,
                   VkFormat vk_format,
                   texture_format fmt);
    texture_data*
    create_texture(const utils::id& texture_id,
                   vk_utils::vulkan_image_sptr image,
                   vk_utils::vulkan_image_view_sptr view);

    // Replace an existing texture's image data in-place — same bindless slot,
    // safe for in-flight frames.
    void
    update_texture(texture_data* td,
                   const utils::buffer& data,
                   uint32_t w,
                   uint32_t h,
                   VkFormat vk_format,
                   texture_format fmt);

    render_pass*
    get_render_pass(const utils::id& id);

    uint32_t
    get_width() const
    {
        return m_width;
    }

    uint32_t
    get_height() const
    {
        return m_height;
    }

    // Active config — what the renderer is currently using. UI/tools should
    // mutate via get_pending_render_config() instead, so the change picks up
    // the apply_pending_render_config() gate that handles topology rebuilds
    // and frame-boundary timing. Direct write access kept for tests and
    // internal callers that intentionally bypass the apply pipeline.
    render_config&
    get_render_config()
    {
        return m_render_config;
    }

    const render_config&
    get_render_config() const
    {
        return m_render_config;
    }

    // ID of the render pass that writes the final swapchain image. This is
    // "composite" when render_scale is enabled and "main" otherwise. Tests
    // and tools that read back the final framebuffer should resolve the pass
    // through this accessor instead of hardcoding the name.
    utils::id
    get_host_pass_id() const
    {
        return m_render_config.render_scale.enabled ? AID("composite") : AID("main");
    }

    // Pending config — UI/tools mutate this freely. Picked up by
    // apply_pending_render_config() at a safe point. The non-const accessor is
    // the mutation entry point, so it raises the dirty flag that
    // has_pending_render_config() reports; the engine drains the render pipeline
    // to idle before applying (apply writes m_render_config, which the render
    // thread reads). Read-only callers must use the const overload to avoid a
    // spurious pipeline stall.
    render_config&
    get_pending_render_config()
    {
        m_render_config_dirty = true;
        return m_pending_render_config;
    }

    const render_config&
    get_pending_render_config() const
    {
        return m_pending_render_config;
    }

    // True when get_pending_render_config() handed out a mutable ref since the
    // last apply. Main-thread-only flag (config is mutated and applied on the
    // main thread; the render thread only reads m_render_config).
    bool
    has_pending_render_config() const
    {
        return m_render_config_dirty;
    }

    // Diff pending vs active and apply changes. Topology fields trigger the
    // appropriate reconfigure_*; other fields are simple value copies. Must
    // be called outside any rendering work — vkDeviceWaitIdle is invoked
    // when topology changes are needed, and it writes m_render_config which the
    // render thread reads, so the caller must ensure the render thread is idle.
    void
    apply_pending_render_config();

    // Render-thread-published per-frame stats, mirrored into atomics at the end
    // of draw_main so the main thread can sample them for the ImGui overlay
    // without racing the render thread's working counters / object cache.
    uint32_t
    stat_all_draws() const
    {
        return m_published_all_draws.load(std::memory_order_relaxed);
    }
    uint32_t
    stat_culled_draws() const
    {
        return m_published_culled_draws.load(std::memory_order_relaxed);
    }
    uint32_t
    stat_objects() const
    {
        return m_published_objects.load(std::memory_order_relaxed);
    }

    float
    get_cascade_split_depth(uint32_t cascade_idx) const
    {
        return m_shadow_config.directional.cascades[cascade_idx].split_depth;
    }

    uint32_t
    get_all_draws() const
    {
        return m_all_draws;
    }

    VkDescriptorSetLayout
    get_bindless_layout() const
    {
        return m_bindless_layout;
    }

    VkDescriptorSet
    get_bindless_set() const
    {
        return m_bindless_set;
    }

    uint32_t
    get_culled_draws() const
    {
        return m_culled_draws;
    }

    // Apply runtime config changes (cluster reinit, etc.)
    void
    apply_config_changes();

    // --- Render-thread task queue (orchestrated by frame_pipeline) ----------
    // Run a task ON THE RENDER THREAD, at the top of its next frame — render-state
    // access held, serialized against drain/draw. RPC handlers reading render state
    // (cache, camera, device stats) use this so they never race the render thread.
    // Blocks the caller (an RPC I/O thread) until the task has run, or until timeout
    // (returns false). The queue lives here, on the render-thread subsystem; the
    // frame pipeline owns the loop and calls drain_render_actions() each turn. The
    // render-thread mirror of vulkan_engine's main-action queue.
    bool
    wait_render_action(std::function<void()> a,
                       std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // [render thread] Run all queued render-thread tasks. Called by the frame
    // pipeline at the top of each render-loop turn (and on its shutdown wake).
    void
    drain_render_actions();

    // Run `a` in a render-state-access context, blocking until done. If the
    // CALLING thread already holds render access (the render thread inside a
    // drain, or main during init/headless/single-threaded teardown) the task
    // runs inline -- this is what makes the call safe at any lifecycle stage
    // and re-entrant from render actions. Otherwise it rides the render-action
    // queue (wait_render_action). For WORKER threads only (e.g. the bake
    // action thread): the queue drains when main submits the next frame, so a
    // mid-stream MAIN-thread caller would stall until timeout -- that's
    // asserted against. Main-thread work that needs the render thread goes
    // through vulkan_engine::post_render_round_trip instead. Returns false on
    // timeout.
    bool
    run_on_render_thread(std::function<void()> a,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // Fire-and-forget render-context work: runs inline when the caller holds
    // render access (render thread / headless / teardown), otherwise rides
    // the render-action queue and executes at the top of a render-loop turn.
    // For self-contained mutations that need no result (e.g. freeing a system
    // pool slot by value-captured handle) -- safe from ANY thread, including
    // main mid-stream, because nothing waits.
    void
    post_render_action(std::function<void()> a);

    // Re-stamp every render-thread-owned pool guard (the three system
    // allocators + all loader storages) to the calling thread at a lifecycle
    // handoff. init() builds system resources on the init/main thread, which
    // lazily pins ownership there; the render loop calls this once at start
    // (paired with render::set_render_access(true)), and main calls it again
    // after the join for single-threaded teardown. Defined in the .cpp -- it
    // reaches into the loader, fwd-declared here.
    void
    bind_render_pools_to_current_thread();

private:
    // Enqueue a render-thread task (just stores it; the loop drains at frame top).
    // Private: the only caller is wait_render_action — no fire-and-forget render
    // path exists.
    void
    queue_render_action(std::function<void()> a);

    void
    render_frame(VkCommandBuffer cmd,
                 frame_state& current_frame,
                 uint32_t swapchain_image_index,
                 uint32_t width,
                 uint32_t height);

    void
    draw_objects_instanced(render::frame_state& frame);

    void
    draw_multi_pipeline_objects_queue(render_line_container& r,
                                      VkCommandBuffer cmd,
                                      render::frame_state& current_frame);

    void
    draw_objects_queue(render_line_container& r,
                       VkCommandBuffer cmd,
                       render::frame_state& current_frame,
                       bool outlined);

    void
    draw_same_pipeline_objects_queue(VkCommandBuffer cmd,
                                     const pipeline_ctx& pctx,
                                     const render_line_container& r,
                                     bool rebind_images = false);

    void
    draw_object(VkCommandBuffer cmd,
                const pipeline_ctx& pctx,
                const render::vulkan_render_data* obj);

    void
    draw_grid(VkCommandBuffer cmd, render::frame_state& current_frame);

    // Editor-only overlay: debug bucket (light gizmo billboards via LAYER_EDITOR_ONLY),
    // wireframe light cubes, and the world grid. Drawn at full res — called from
    // the composite pass when render-scale is on, otherwise from the main pass.
    void
    draw_debug_overlay(VkCommandBuffer cmd, render::frame_state& current_frame);

    void
    draw_selection_mask(VkCommandBuffer cmd, render::frame_state& current_frame);

    void
    draw_outline_post(VkCommandBuffer cmd, render::frame_state& current_frame);

    void
    prepare_debug_light_data(render::frame_state& current_frame);
    void
    draw_debug_lights(VkCommandBuffer cmd, render::frame_state& current_frame);

    // Editor-only (composites the ImGui UI target); compiled out of game builds.
#if KRG_HAS_IMGUI
    void
    draw_ui_overlay(VkCommandBuffer cmd, render::frame_state& current_frame);
#endif

    // Nearest-neighbor upscale of scene_lowres_target to full-res swapchain.
    // Only called from the composite pass when render_config.render_scale.enabled is true.
    void
    draw_scene_upscale(VkCommandBuffer cmd, render::frame_state& current_frame);

    // Composite pass entry — nearest upscale + UI overlay. Only called when
    // render_config.render_scale.enabled is true.
    void
    draw_composite(VkCommandBuffer cmd, render::frame_state& current_frame);

    // Depth-edge outline (composite pass). Only drawn when
    // render_config.outline.enabled is true.
    void
    draw_depth_outline(VkCommandBuffer cmd, render::frame_state& current_frame);

    void
    prepare_draw_resources(render::frame_state& frame);

    // clang-format off
    void upload_obj_data(render::frame_state& frame);
    void upload_universal_light_data(render::frame_state& frame);
    void upload_directional_light_data(render::frame_state& frame);
    void upload_material_data(render::frame_state& frame);
    void upload_bone_matrices(render::frame_state& frame);
    void upload_probe_data(render::frame_state& frame);
    // clang-format on

    // Instance drawing methods
    void
    prepare_instance_data(render::frame_state& frame);

    void
    build_batches_for_queue(render_line_container& r, bool outlined);

    void
    build_batches_for_queue_into(render_line_container& r,
                                 bool outlined,
                                 std::vector<draw_batch>& out_batches,
                                 bool apply_frustum_cull);

    void
    upload_instance_slots(render::frame_state& frame);

    // GPU compute cluster culling
    void
    init_cluster_cull_compute();

    void
    dispatch_cluster_cull_impl(VkCommandBuffer cmd);

    // GPU compute frustum culling
    void
    init_frustum_cull_compute();

    void
    dispatch_frustum_cull_impl(VkCommandBuffer cmd);

    void
    upload_frustum_data(render::frame_state& frame);

    void
    bind_mesh(VkCommandBuffer cmd, mesh_data* cur_mesh);

    void
    draw_mesh(VkCommandBuffer cmd,
              mesh_data* m,
              uint32_t instance_count = 1,
              uint32_t first_instance = 0);

    void
    bind_bindless(VkCommandBuffer cmd, VkPipelineLayout layout);

    void
    draw_fullscreen_quad(VkCommandBuffer cmd, shader_effect_data* se, const void* push_data);

    void
    bind_global_descriptors(VkCommandBuffer cmd, render::frame_state& current_frame);

    void
    bind_material(VkCommandBuffer cmd,
                  material_data* cur_material,
                  render::frame_state& current_frame,
                  pipeline_ctx& ctx,
                  bool outline = false);

    void
    push_config(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout, uint32_t mat_id);

    // clang-format off
    void upload_gpu_object_data(gpu::object_data* object_SSBO);
    void upload_gpu_universal_light_data(gpu::universal_light_data* lights_SSBO);
    void upload_gpu_directional_light_data(gpu::directional_light_data* lights_SSBO);
    void upload_gpu_materials_data(uint8_t* object_SSBO, materials_update_queue& queue);
    // clang-format on

    void
    update_transparent_objects_queue();

    frame_state&
    get_current_frame_transfer_data();

    void
    prepare_render_passes();
    void
    prepare_pass_bindings();
    void
    prepare_system_resources();
    void
    prepare_ui_pipeline();
    void
    prepare_ui_resources();
    void
    prepare_scene_upscale_pipeline();

    // Static samplers
    void
    init_static_samplers();
    void
    deinit_static_samplers();

    // Bindless textures
    void
    init_bindless_textures();
    void
    deinit_bindless_textures();
    void
    update_bindless_descriptors();

    void
    update_ui(frame_state& cmd);
    void
    draw_ui(frame_state& cmd);

    // Latch the main-thread-published camera into m_camera_data + m_frustum on
    // the render thread. Called at the start of draw_main/draw_headless so the
    // camera and frustum are render-thread-owned for the frame.
    void
    apply_pending_camera();
    void
    resize(uint32_t width, uint32_t height);

    void
    setup_render_graph();

    // Allocate the full per-frame GPU buffer set for frame slot i. Called by
    // init for each live slot and by reconfigure_swapchain when the in-flight
    // count grows.
    void
    create_frame_buffers(size_t i);

    // Seed a freshly created frame slot (`dst`) from an existing live slot
    // (`src`) instead of re-deriving from the model caches. Clones the
    // persistent SSBO bytes (the scene state `src` has already applied) and
    // copies `src`'s still-pending upload queues. Together they reconstruct the
    // full scene: cloned bytes + queued deltas = everything. Used by
    // reconfigure_swapchain when the in-flight count grows. Transient per-frame
    // buffers are rewritten every frame, so they don't need seeding.
    void
    seed_frame_slot_from(uint32_t dst, uint32_t src);

    void
    setup_instanced_render_graph();

    // Shadow mapping
    void
    init_shadow_passes();
    void
    init_shadow_resources();
    void
    upload_shadow_data(render::frame_state& frame);
    void
    compute_cascade_splits(float near, float far, float lambda);
    void
    compute_shadow_matrices();
    void
    draw_shadow_atlas(VkCommandBuffer cmd);
    void
    draw_shadow_pass(VkCommandBuffer cmd, uint32_t cascade_idx);
    void
    draw_shadow_local_pass(VkCommandBuffer cmd, uint32_t shadow_idx, bool back_face);
    void
    select_shadowed_lights();
    void
    compute_shadow_atlas_layout();

    uint32_t m_all_draws = 0;
    uint32_t m_culled_draws = 0;

    // Render-thread → main-thread published copies of the above plus the render
    // object count, stored at the end of draw_main (see stat_*()). Atomic
    // because the main thread reads them concurrently with the next frame's draw.
    std::atomic<uint32_t> m_published_all_draws{0};
    std::atomic<uint32_t> m_published_culled_draws{0};
    std::atomic<uint32_t> m_published_objects{0};

    // Render-thread task queue (RPC introspection). Pushed from an RPC I/O thread
    // (queue_render_action), drained on the render thread by the frame pipeline
    // (drain_render_actions) — hence the mutex.
    std::vector<std::function<void()>> m_render_actions;
    std::mutex m_render_actions_mutex;

    // Render thread's working camera + frustum, written ONLY by
    // apply_pending_camera() at draw start (render-owned). All render passes
    // read these.
    gpu::camera_data m_camera_data;

    // Camera written by set_camera() (main thread, into m_camera_pending[
    // m_build_frame_slot]) and latched into m_camera_data by apply_pending_camera()
    // (render thread, from m_camera_pending[m_draw_frame_slot]). Double-buffered
    // by frame parity so the main thread building frame F+1 writes a different
    // slot than the render thread reads for frame F.
    gpu::camera_data m_camera_pending[2];

    // Frame-parity slot selectors for the camera/UI double buffers. m_build_frame_slot
    // is written only by the main thread (begin_frame_inputs); m_draw_frame_slot
    // only by the render thread (set_draw_frame_slot, stamped by the render loop).
    // The pipeline gate guarantees they never name the same slot concurrently, so
    // no atomics are needed — the m_render_mutex handoff supplies happens-before.
    uint32_t m_build_frame_slot = 0;
    uint32_t m_draw_frame_slot = 0;

    glm::vec3 m_last_camera_position = glm::vec3{0.f};

    std::unordered_map<std::string, render_line_container> m_default_render_object_queue;

    std::unordered_map<std::string, render_line_container> m_outline_render_object_queue;

    render_line_container m_transparent_render_object_queue;

    std::unordered_map<std::string, render_line_container> m_debug_render_object_queue;

    utils::id_allocator m_selected_material_alloc;

    buffer_layout<material_data*> m_materials_layout;

    std::vector<frame_state> m_frames;

    // Double-buffered ImGui draw-data snapshots, indexed by frame parity. Main
    // thread fills m_ui_snapshots[m_build_frame_slot] in capture_ui_snapshot();
    // the render thread reads m_ui_snapshots[m_draw_frame_slot]. Two buffers
    // are enough because the main thread runs at most one frame ahead.
    ui_draw_snapshot m_ui_snapshots[2];

    // Number of frame slots whose GPU buffers are currently allocated. m_frames
    // is sized to the (max) device frame count; only [0, m_allocated_frame_slots)
    // hold live buffers — the rest are freed to reclaim memory when
    // frames_in_flight is lowered (see reconfigure_swapchain).
    uint32_t m_allocated_frame_slots = 0;

    // UI
    shader_effect_data* m_ui_se = nullptr;
    shader_effect_data* m_ui_copy_se = nullptr;

    texture_data* m_ui_txt = nullptr;
    texture_data* m_ui_target_txt = nullptr;
    material_data* m_ui_mat = nullptr;
    material_data* m_ui_target_mat = nullptr;

    // Mirrors the PushConstants block in se_uioverlay.vert/.frag. scale/translate
    // are consumed by the vertex stage; tex_index/sampler_index by the fragment
    // stage to sample the font from the global bindless set.
    struct ui_push_constants
    {
        glm::vec2 scale;
        glm::vec2 translate;
        uint32_t tex_index = 0;
        uint32_t sampler_index = 0;
    };
    ui_push_constants m_ui_push_constants;

    // Grid
    shader_effect_data* m_grid_se = nullptr;
    material_data* m_grid_mat = nullptr;

    // Selection mask + outline post-process
    shader_effect_data* m_selection_mask_se = nullptr;
    material_data* m_selection_mask_mat = nullptr;
    shader_effect_data* m_outline_post_se = nullptr;
    material_data* m_outline_post_mat = nullptr;
    uint32_t m_selection_mask_bindless_idx = 0xFFFFFFFFu;
    // Bindless wrapper around the selection-mask pass's color image (allocated
    // once in prepare_system_resources; image/view refreshed on reconfigure).
    texture_data* m_selection_mask_txt = nullptr;

    // Low-res scene target + composite pass for render_scale mode.
    // Populated only when m_render_config.render_scale.enabled is true.
    std::vector<vk_utils::vulkan_image_sptr> m_scene_lowres_images;
    std::vector<vk_utils::vulkan_image_view_sptr> m_scene_lowres_views;
    render_pass_sptr m_composite_pass;
    shader_effect_data* m_scene_upscale_se = nullptr;
    material_data* m_scene_upscale_mat = nullptr;
    texture_data* m_scene_upscale_txt = nullptr;
    uint32_t m_scene_lowres_width = 0;
    uint32_t m_scene_lowres_height = 0;

    // Depth-based silhouette outline (composite pass). Populated only when
    // m_render_config.outline.enabled && m_render_config.render_scale.enabled.
    shader_effect_data* m_depth_outline_se = nullptr;
    uint32_t m_scene_depth_bindless_idx = 0xFFFFFFFFu;
    // Bindless wrapper around main-pass depth (render_scale paths). Allocated
    // on first need; image/view swapped in place across reconfigures so the
    // bindless slot stays stable.
    texture_data* m_scene_depth_txt = nullptr;

    // BDA per-frame tracking — ensures per-draw BDA addresses are set before use
    bool m_bda_material_bound = false;

    // Descriptors

    VkDescriptorSet m_objects_set = VK_NULL_HANDLE;
    VkDescriptorSet m_global_set = VK_NULL_HANDLE;

    // Static samplers (8 sampler variants for runtime selection)
    VkSampler m_static_samplers[KGPU_SAMPLER_COUNT] = {};

    // Bindless textures
    VkDescriptorPool m_bindless_pool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_bindless_layout = VK_NULL_HANDLE;
    VkDescriptorSet m_bindless_set = VK_NULL_HANDLE;
    textures_update_queue m_global_textures_queue;  // Global queue (not per-frame) for bindless
                                                    // updates

    gpu::push_constants_main m_obj_config = {};
    gpu::push_constants_shadow m_shadow_pc = {};
    gpu::push_constants_grid m_grid_pc = {};

    // Scene/resource storage lives on render_system.loader (formerly a local
    // render_cache member); bound at init() before any storage access.
    vulkan_render_loader* m_loader = nullptr;

    // System (render-created) mesh/material allocators; bind to k_system_lane
    // of the loader's MERGED mesh/material storages at init (render_translator's
    // content allocators own k_content_lane of the same storages). The
    // renderer owns these render-internal pools' identity; the storage stays
    // with the loader (the allocator holds only a dispatch-token pointer).
    render::types::mesh_allocator m_system_meshes_alloc;
    render::types::material_allocator m_system_materials_alloc;
    // Bindless pool: single-lane storage of texture_data BY VALUE (distinct
    // from the content texture_data* handle map), so its own allocator alias.
    render::types::bindless_texture_allocator m_bindless_textures_alloc;

    // Clustered lighting
    cluster_grid m_cluster_grid;
    gpu::cluster_grid_data m_cluster_config;
    bool m_clusters_dirty = true;

    // GPU compute cluster culling
    render_pass_sptr m_cluster_cull_pass;
    compute_shader_data* m_cluster_cull_shader = nullptr;  // owned by m_cluster_cull_pass
    VkDescriptorSet m_cluster_cull_descriptor_set = VK_NULL_HANDLE;

    // GPU compute frustum culling
    render_pass_sptr m_frustum_cull_pass;
    compute_shader_data* m_frustum_cull_shader = nullptr;  // owned by m_frustum_cull_pass
    VkDescriptorSet m_frustum_cull_descriptor_set = VK_NULL_HANDLE;
    bool m_gpu_frustum_culling_enabled = true;

    // Frustum for view culling
    frustum m_frustum;

    // Object-level BVH for CPU raycasting (picking)
    object_bvh m_object_bvh;
    bool m_object_bvh_dirty = true;

    // Shadow mapping
    gpu::shadow_config_data m_shadow_config = {};

    // Render config (loaded from render.acfg)
    render_config m_render_config;
    render_config m_pending_render_config;
    // Raised when get_pending_render_config() hands out a mutable ref; cleared
    // by apply_pending_render_config(). Gates the pipeline-drain-before-apply in
    // the engine loop. Main-thread-only (see has_pending_render_config()).
    bool m_render_config_dirty = false;

    // Snapshots of last-applied config (to detect runtime changes)
    render_config::cluster_cfg m_applied_clusters;
    uint32_t m_applied_shadow_atlas_size = 0;
    uint32_t m_applied_shadow_csm_tile_size = 0;
    uint32_t m_applied_shadow_local_tile_size = 0;
    bool m_applied_shadow_depth_16bit = false;
    shader_effect_data* m_debug_wire_se = nullptr;
    material_data* m_debug_wire_mat = nullptr;
    mesh_data* m_debug_sphere_mesh = nullptr;
    mesh_data* m_debug_cone_mesh = nullptr;
    // Procedural fullscreen quad for post/UI passes. Owned render-side, addressed
    // directly (no id lookup). Distinct from the content "plane_mesh" asset.
    mesh_data* m_fullscreen_quad = nullptr;
    uint32_t m_debug_light_draw_count = 0;
    uint32_t m_debug_light_instance_base = 0;
    render_pass_sptr m_shadow_atlas_pass;
    uint32_t m_shadow_atlas_bindless_index = 0;

    struct shadow_atlas_tile
    {
        uint32_t x, y, size;
        glm::vec2 uv_offset, uv_scale;
    };
    shadow_atlas_tile m_csm_tiles[KGPU_CSM_CASCADE_COUNT];
    shadow_atlas_tile m_local_tiles[KGPU_MAX_SHADOWED_LOCAL_LIGHTS * 2];

    shader_effect_data* m_shadow_se = nullptr;
    shader_effect_data* m_shadow_dpsm_se = nullptr;

    // Selected directional light (handle; index == GPU slot)
    render::types::directional_light_handle m_selected_directional_light;

    // Instance drawing state
    std::vector<uint32_t> m_instance_slots_staging;  // CPU-side staging for slots
    std::vector<draw_batch> m_draw_batches;          // Pre-computed batches for frame
    std::vector<draw_batch> m_debug_draw_batches;    // Debug object batches (unlit, no shadows)
    // Per-pass shadow caster batches, culled against each light's volume (#7).
    // Each pass appends only its visible slots into m_instance_slots_staging, so
    // its batches index compact, contiguous ranges for instanced draws.
    std::array<std::vector<draw_batch>, KGPU_CSM_CASCADE_COUNT> m_cascade_shadow_batches;
    std::array<std::vector<draw_batch>, KGPU_MAX_SHADOWED_LOCAL_LIGHTS * 2> m_local_shadow_batches;

    // CPU-side cull data for local lights, filled by select_shadowed_lights and
    // consumed by prepare_instance_data (sphere + hemisphere test for points).
    struct local_shadow_cull_info
    {
        glm::vec3 position{0.0f};
        float radius = 0.0f;
        glm::vec3 front_dir{0.0f, 0.0f, 1.0f};
        uint32_t type = 0;
    };
    std::array<local_shadow_cull_info, KGPU_MAX_SHADOWED_LOCAL_LIGHTS> m_local_shadow_cull;

    // Reusable scratch for select_shadowed_lights ranking (avoids a per-frame heap
    // alloc/free). Cleared (capacity retained) at the start of each selection pass.
    struct shadow_candidate
    {
        uint32_t light_slot;
        float contribution;
    };
    std::vector<shadow_candidate> m_shadow_candidates;

    // Bone matrix staging for skeletal animation
    std::vector<glm::mat4> m_bone_matrices_staging;

    // Light probes — bulk replacement (no per-element queue). stage_set_probes
    // updates the cache and seeds m_probes_pending_uploads with FRAMES_IN_FLIGHT
    // so each frame's SSBO picks up the new payload at its next prepare pass.
    std::vector<gpu::sh_probe> m_probes;
    gpu::probe_grid_config m_probe_grid_config{};
    uint32_t m_probes_pending_uploads = 0;

    // Render graph
    vulkan_render_graph m_render_graph;

    // Current frame state pointer (used by render graph callbacks)
    frame_state* m_current_frame = nullptr;

    // Render target dimensions — match the swapchain image extent.
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};
}  // namespace render
}  // namespace kryga
