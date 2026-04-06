#pragma once

#include "vulkan_render/types/vulkan_render_data.h"
#include "vulkan_render/types/vulkan_gpu_types.h"
#include "vulkan_render/types/vulkan_light_data.h"
#include "vulkan_render/types/vulkan_compute_shader_data.h"
#include "vulkan_render/utils/vulkan_buffer.h"
#include "vulkan_render/utils/segments.h"
#include "vulkan_render/types/vulkan_render_pass.h"
#include "vulkan_render/render_cache.h"
#include "vulkan_render/vulkan_render_graph.h"
#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/render_enums.h"
#include "vulkan_render/render_config.h"
#include "render/utils/frustum.h"
#include "render/utils/cluster_grid.h"
#include "render/utils/light_grid.h"
#include "gpu_types/gpu_cluster_types.h"
#include "gpu_types/gpu_shadow_types.h"

#include <utils/check.h>
#include <utils/id.h>
#include <utils/line_container.h>
#include <utils/id_allocator.h>

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

    void
    set_camera(gpu::camera_data d);

    void
    draw_main();

    void
    draw_headless();

    // clang-format off
    void schd_add_object(render::vulkan_render_data* obj_data);
    void schd_add_material(render::material_data* mat_data);
    void schd_add_light(render::vulkan_directional_light_data* ld);
    void schd_add_light(render::vulkan_universal_light_data* ld);

    void schd_update_object(render::vulkan_render_data* obj_data);
    void schd_update_object_queue(render::vulkan_render_data* obj_data);
    void schd_update_material(render::material_data* mat_data);
    void schd_update_light(render::vulkan_directional_light_data* ld);
    void schd_update_light(render::vulkan_universal_light_data* ld);
    void schd_update_texture(render::texture_data* tex);

    void schd_remove_object(render::vulkan_render_data* obj_data);
    void schd_remove_material(render::material_data* mat_data);
    // clang-format on

    void
    set_selected_directional_light(const utils::id& id);

    uint32_t
    get_selected_directional_light_slot();

    std::vector<glm::mat4>&
    get_bone_matrices_staging()
    {
        return m_bone_matrices_staging;
    }

    void
    clear_upload_queue();

    vulkan_render_data*
    object_id_under_coordinate(uint32_t x, uint32_t y);

    render_cache&
    get_cache();

    render_pass*
    get_render_pass(const utils::id& id);

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

    uint32_t
    get_culled_draws() const
    {
        return m_culled_draws;
    }

    render_mode
    get_render_mode() const
    {
        return m_render_mode;
    }

    bool
    is_instanced_mode() const
    {
        return m_render_mode == render_mode::instanced;
    }

    // Apply runtime config changes (cluster reinit, render mode switch, etc.)
    void
    apply_config_changes();

private:
    void
    render_frame(VkCommandBuffer cmd,
                 frame_state& current_frame,
                 uint32_t swapchain_image_index,
                 uint32_t width,
                 uint32_t height);

    // Mode-specific drawing functions
    void
    draw_objects_instanced(render::frame_state& frame);

    void
    draw_objects_per_object(render::frame_state& frame);

    void
    draw_picking_instanced(VkCommandBuffer cmd);

    void
    draw_picking_per_object(VkCommandBuffer cmd);

    void
    draw_grid(VkCommandBuffer cmd, render::frame_state& current_frame);

    void
    prepare_debug_light_data(render::frame_state& current_frame);
    void
    draw_debug_lights(VkCommandBuffer cmd, render::frame_state& current_frame);

    void
    draw_ui_overlay(VkCommandBuffer cmd, render::frame_state& current_frame);

    void
    prepare_draw_resources(render::frame_state& frame);

    // clang-format off
    void upload_obj_data(render::frame_state& frame);
    void upload_universal_light_data(render::frame_state& frame);
    void upload_directional_light_data(render::frame_state& frame);
    void upload_material_data(render::frame_state& frame);
    void upload_bone_matrices(render::frame_state& frame);
    // clang-format on

    // Clustered lighting methods
    void
    build_light_clusters();

    void
    upload_cluster_data(render::frame_state& frame);

    // Instance drawing methods
    void
    prepare_instance_data(render::frame_state& frame);

    void
    build_batches_for_queue(render_line_container& r, bool outlined);

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

    // Per-object light grid methods
    void
    rebuild_light_grid();

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
                                     bool rebind_images = true);

    void
    draw_object(VkCommandBuffer cmd,
                const pipeline_ctx& pctx,
                const render::vulkan_render_data* obj);

    void
    bind_mesh(VkCommandBuffer cmd, mesh_data* cur_mesh);

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
    void
    resize(uint32_t width, uint32_t height);

    void
    setup_render_graph();

    void
    setup_instanced_render_graph();

    void
    setup_per_object_render_graph();

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
    draw_shadow_pass(VkCommandBuffer cmd, uint32_t cascade_idx);
    void
    draw_shadow_local_pass(VkCommandBuffer cmd, uint32_t shadow_idx, bool back_face);
    void
    select_shadowed_lights();

    uint32_t m_all_draws = 0;
    uint32_t m_culled_draws = 0;

    gpu::camera_data m_camera_data;

    glm::vec3 m_last_camera_position = glm::vec3{0.f};

    std::unordered_map<std::string, render_line_container> m_default_render_object_queue;

    std::unordered_map<std::string, render_line_container> m_outline_render_object_queue;

    render_line_container m_transparent_render_object_queue;

    utils::id_allocator m_selected_material_alloc;

    buffer_layout<material_data*> m_materials_layout;

    std::vector<frame_state> m_frames;

    // UI
    shader_effect_data* m_ui_se = nullptr;
    shader_effect_data* m_ui_copy_se = nullptr;

    texture_data* m_ui_txt = nullptr;
    texture_data* m_ui_target_txt = nullptr;
    material_data* m_ui_mat = nullptr;
    material_data* m_ui_target_mat = nullptr;

    struct ui_push_constants
    {
        glm::vec2 scale;
        glm::vec2 translate;
    };
    ui_push_constants m_ui_push_constants;

    // Grid
    shader_effect_data* m_grid_se = nullptr;
    material_data* m_grid_mat = nullptr;

    // BDA per-frame tracking — ensures per-draw BDA addresses are set before use
    bool m_bda_material_bound = false;

    // Generic
    material_data* m_outline_mat = nullptr;
    material_data* m_pick_mat = nullptr;

    // Descriptors

    VkDescriptorSet m_objects_set = VK_NULL_HANDLE;
    VkDescriptorSet m_global_set = VK_NULL_HANDLE;

    // Static samplers (7 sampler variants for runtime selection)
    VkSampler m_static_samplers[7] = {};  // KGPU_SAMPLER_COUNT

    // Bindless textures
    VkDescriptorPool m_bindless_pool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_bindless_layout = VK_NULL_HANDLE;
    VkDescriptorSet m_bindless_set = VK_NULL_HANDLE;

    gpu::push_constants_main m_obj_config = {};
    gpu::push_constants_shadow m_shadow_pc = {};
    gpu::push_constants_pick m_pick_pc = {};
    gpu::push_constants_grid m_grid_pc = {};

    render_cache m_cache;

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

    // Per-object light grid (alternative to clustered)
    light_grid m_light_grid;
    bool m_light_grid_dirty = true;

    // Frustum for view culling
    frustum m_frustum;

    // Shadow mapping
    gpu::shadow_config_data m_shadow_config = {};

    // Render config (loaded from render.acfg)
    render_config m_render_config;

    // Snapshots of last-applied config (to detect runtime changes)
    render_config::cluster_cfg m_applied_clusters;
    uint32_t m_applied_shadow_map_size = 0;
    shader_effect_data* m_debug_wire_se = nullptr;
    material_data* m_debug_wire_mat = nullptr;
    uint32_t m_debug_light_draw_count = 0;
    uint32_t m_debug_light_instance_base = 0;
    render_pass_sptr m_shadow_passes[KGPU_CSM_CASCADE_COUNT];
    render_pass_sptr m_shadow_local_passes[KGPU_MAX_SHADOWED_LOCAL_LIGHTS * 2];  // *2 for DPSM
    uint32_t m_shadow_map_bindless_indices[KGPU_CSM_CASCADE_COUNT][FRAMES_IN_FLIGHT] = {};
    uint32_t m_shadow_local_bindless_indices[KGPU_MAX_SHADOWED_LOCAL_LIGHTS * 2][FRAMES_IN_FLIGHT] =
        {};
    shader_effect_data* m_shadow_se = nullptr;
    shader_effect_data* m_shadow_dpsm_se = nullptr;

    // Selected directional light
    utils::id m_selected_directional_light_id;

    // Render mode (set at init, determines graph configuration)
    render_mode m_render_mode = render_mode::instanced;

    // Instance drawing state
    std::vector<uint32_t> m_instance_slots_staging;  // CPU-side staging for slots
    std::vector<draw_batch> m_draw_batches;          // Pre-computed batches for frame

    // Bone matrix staging for skeletal animation
    std::vector<glm::mat4> m_bone_matrices_staging;

    // Render graph
    vulkan_render_graph m_render_graph;

    // Current frame state pointer (used by render graph callbacks)
    frame_state* m_current_frame = nullptr;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
};
}  // namespace render
}  // namespace kryga
