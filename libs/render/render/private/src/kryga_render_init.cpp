#include "vulkan_render/kryga_render.h"

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/vulkan_render_loader_create_infos.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_render_pass_builder.h"
#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/utils/vulkan_debug.h"

#include <render/utils/mesh_primitives.h>

#include <gpu_types/gpu_generic_constants.h>
#include <gpu_types/gpu_vertex_types.h>
#include <gpu_types/gpu_frustum_types.h>
#include <gpu_types/dynamic_layout/gpu_types.h>
#include <gpu_types/gpu_shadow_types.h>
#include <gpu_types/gpu_probe_types.h>

#include <utils/kryga_log.h>
#include <utils/buffer.h>

#include <vfs/vfs.h>
#include <vfs/io.h>
#include <global_state/global_state.h>
#include <vulkan_render/render_system.h>

#include <shader_system/shader_loader.h>

#include <tracy/Tracy.hpp>

#include <cmath>
#include <kryga_port/format.h>
#include <string>

namespace kryga
{
namespace render
{
namespace
{

const uint32_t INITIAL_MATERIAL_SEGMENT_RANGE_SIZE = 1024;
const uint32_t INITIAL_MATERIAL_RANGE_SIZE = 10 * INITIAL_MATERIAL_SEGMENT_RANGE_SIZE;

const uint32_t OBJECTS_BUFFER_SIZE = 16 * 1024;
const uint32_t UNIVERSAL_LIGHTS_BUFFER_SIZE = 1024;
const uint32_t DIRECT_LIGHTS_BUFFER_SIZE = 512;

const uint32_t CAMERA_UBO_SIZE = 4 * 1024;

uint32_t
compute_cluster_counts_size(uint32_t screen_w,
                            uint32_t screen_h,
                            const render_config::cluster_cfg& cfg)
{
    uint32_t tiles_x = (screen_w + cfg.tile_size - 1) / cfg.tile_size;
    uint32_t tiles_y = (screen_h + cfg.tile_size - 1) / cfg.tile_size;
    uint32_t total = tiles_x * tiles_y * cfg.depth_slices;
    return total * sizeof(uint32_t);
}

uint32_t
compute_cluster_indices_size(uint32_t screen_w,
                             uint32_t screen_h,
                             const render_config::cluster_cfg& cfg)
{
    uint32_t tiles_x = (screen_w + cfg.tile_size - 1) / cfg.tile_size;
    uint32_t tiles_y = (screen_h + cfg.tile_size - 1) / cfg.tile_size;
    uint32_t total = tiles_x * tiles_y * cfg.depth_slices;
    return total * cfg.max_lights_per_cluster * sizeof(uint32_t);
}

}  // namespace

vulkan_render::vulkan_render()
{
}

vulkan_render::~vulkan_render()
{
}

void
vulkan_render::create_frame_buffers(size_t i)
{
    auto& device = glob::glob_state().getr_render().device;
    const auto& clusters = m_render_config.clusters;

    m_frames[i].buffers.objects = device.create_buffer(OBJECTS_BUFFER_SIZE,
                                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                       VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                       0,
                                                       KRG_VK_FMT_NAME("frame_{}.objects", i));

    m_frames[i].buffers.materials = device.create_buffer(INITIAL_MATERIAL_RANGE_SIZE,
                                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                         VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                         0,
                                                         KRG_VK_FMT_NAME("frame_{}.materials", i));

    m_frames[i].buffers.universal_lights =
        device.create_buffer(UNIVERSAL_LIGHTS_BUFFER_SIZE,
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             0,
                             KRG_VK_FMT_NAME("frame_{}.universal_lights", i));

    m_frames[i].buffers.directional_lights =
        device.create_buffer(DIRECT_LIGHTS_BUFFER_SIZE,
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             0,
                             KRG_VK_FMT_NAME("frame_{}.directional_lights", i));

    m_frames[i].buffers.dynamic_data =
        device.create_buffer(CAMERA_UBO_SIZE,
                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             0,
                             KRG_VK_FMT_NAME("frame_{}.dynamic_data", i));

    // Cluster buffers — sized to actual grid dimensions, auto-grow on config change.
    m_frames[i].buffers.cluster_counts = device.create_buffer(
        compute_cluster_counts_size(m_scene_lowres_width, m_scene_lowres_height, clusters),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        0,
        KRG_VK_FMT_NAME("frame_{}.cluster_counts", i));

    m_frames[i].buffers.cluster_indices = device.create_buffer(
        compute_cluster_indices_size(m_scene_lowres_width, m_scene_lowres_height, clusters),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        0,
        KRG_VK_FMT_NAME("frame_{}.cluster_indices", i));

    m_frames[i].buffers.cluster_config = device.create_buffer(
        sizeof(gpu::cluster_grid_data),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        0,
        KRG_VK_FMT_NAME("frame_{}.cluster_config", i));

    // Instance slots buffer for instanced drawing
    m_frames[i].buffers.instance_slots =
        device.create_buffer(KGPU_initial_instance_slots_size * sizeof(uint32_t),
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             0,
                             KRG_VK_FMT_NAME("frame_{}.instance_slots", i));

    // Bone matrices SSBO for skeletal animation (initial 64KB)
    m_frames[i].buffers.bone_matrices =
        device.create_buffer(64 * 1024,
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             0,
                             KRG_VK_FMT_NAME("frame_{}.bone_matrices", i));

    // GPU frustum culling buffers
    m_frames[i].buffers.frustum_data =
        device.create_buffer(sizeof(gpu::frustum_data),
                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             0,
                             KRG_VK_FMT_NAME("frame_{}.frustum_data", i));

    m_frames[i].buffers.visible_indices =
        device.create_buffer(OBJECTS_BUFFER_SIZE * sizeof(uint32_t),
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             VMA_MEMORY_USAGE_GPU_ONLY,
                             0,
                             KRG_VK_FMT_NAME("frame_{}.visible_indices", i));

    m_frames[i].buffers.cull_output = device.create_buffer(
        sizeof(gpu::cull_output_data) + 64,  // Extra space for alignment
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,  // For vkCmdFillBuffer
        VMA_MEMORY_USAGE_GPU_ONLY,
        0,
        KRG_VK_FMT_NAME("frame_{}.cull_output", i));

    // Shadow data SSBO
    m_frames[i].buffers.shadow_data =
        device.create_buffer(sizeof(gpu::shadow_config_data),
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             0,
                             KRG_VK_FMT_NAME("frame_{}.shadow_data", i));

    // Light probe SSBOs (initial: 1 dummy probe + grid config)
    m_frames[i].buffers.probe_data =
        device.create_buffer(sizeof(gpu::sh_probe),
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             0,
                             KRG_VK_FMT_NAME("frame_{}.probe_data", i));

    m_frames[i].buffers.probe_grid =
        device.create_buffer(sizeof(gpu::probe_grid_config),
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             0,
                             KRG_VK_FMT_NAME("frame_{}.probe_grid", i));
}

void
vulkan_render::seed_frame_slot_from(uint32_t dst, uint32_t src)
{
    auto& s = m_frames[src];
    auto& d = m_frames[dst];

    // Clone the persistent SSBO bytes — the scene state `src` has already
    // applied. All four are STORAGE + CPU_TO_GPU, so a host memcpy clone works.
    // Transient buffers (camera, clusters, culling, shadows, bones, instances,
    // UI) are rewritten every frame before use and need no seeding. Probes use
    // their own bulk-replace counter, re-armed in reconfigure_swapchain.
    constexpr VkBufferUsageFlags storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    d.buffers.objects.clone_contents_from(s.buffers.objects, storage);
    d.buffers.universal_lights.clone_contents_from(s.buffers.universal_lights, storage);
    d.buffers.directional_lights.clone_contents_from(s.buffers.directional_lights, storage);
    d.buffers.materials.clone_contents_from(s.buffers.materials, storage);

    // Copy `src`'s still-pending scheduled updates. Combined with the cloned
    // bytes above the new slot now holds the full scene: cloned bytes = updates
    // `src` already applied; copied queues = updates `src` hasn't applied yet
    // either. `dst` drains them on its next prepare_draw_resources. This also
    // carries the materials_queue_set sizing onto the fresh slot. No per-cache
    // enumeration here, so adding a new renderable type can't silently break
    // the rebuild — it only needs its buffer added to the clone list above and
    // its queue rides along in this struct copy.
    d.uploads = s.uploads;
}

void
vulkan_render::init(uint32_t w, uint32_t h, const render_config& config, bool only_rp)
{
    auto& device = glob::glob_state().getr_render().device;
    auto extent = device.swapchain_extent();
    m_width = extent.width;
    m_height = extent.height;

    m_render_config = config;
    m_pending_render_config = config;

    // Initialize static samplers first - needed for bindless texture layout
    init_static_samplers();

    // Initialize bindless textures early - needed for shader pipeline layouts
    init_bindless_textures();

    prepare_render_passes();
    prepare_pass_bindings();

    if (only_rp)
    {
        return;
    }

    m_frames.resize(device.frame_size());

    for (size_t i = 0; i < m_frames.size(); ++i)
    {
        m_frames[i].frame = &device.frame(i);
    }

    // Allocate per-frame GPU buffers only for the live in-flight slots. The
    // engine keeps frames_in_flight == swapchain image count, so slots
    // [frames_in_flight, frame_size) are never cycled and stay empty until the
    // count is raised (reconfigure_swapchain recreates the swapchain and grows
    // these on demand).
    for (uint32_t i = 0; i < device.frames_in_flight(); ++i)
    {
        create_frame_buffers(i);
    }
    m_allocated_frame_slots = device.frames_in_flight();

    // Sync the config's frames_in_flight + present mode to what the device
    // actually adopted at init (it was constructed with these from render_config,
    // but the driver may have granted a different image count), so the UI
    // reflects reality.
    m_render_config.frames_in_flight = device.frames_in_flight();
    m_pending_render_config.frames_in_flight = device.frames_in_flight();
    m_render_config.present = device.current_present_mode();
    m_pending_render_config.present = device.current_present_mode();

    prepare_system_resources();

#if KRG_HAS_IMGUI
    // Set up ImGui font atlas and UI pipeline. Skipped when headless (no
    // window) or when no ImGui context exists yet.
    if (!device.is_headless() && ImGui::GetCurrentContext())
    {
        prepare_ui_resources();
        prepare_ui_pipeline();
    }
#endif

    // Scene upscale pipeline runs in BOTH editor and game when render_scale is
    // enabled. Independent of ImGui — must live outside the KRG_EDITOR block,
    // otherwise game builds get a composite pass with no upscale shader and
    // present a black swapchain.
    prepare_scene_upscale_pipeline();

    // Initialize clustered lighting (must match camera near/far planes).
    // Uses the effective rendering resolution — when render_scale is on the
    // scene rasters at lowres, so the tile grid must be sized for that pixel
    // grid. m_scene_lowres_* is already set by prepare_render_passes above.
    m_cluster_grid.init(m_scene_lowres_width,
                        m_scene_lowres_height,
                        KGPU_znear,  // near plane
                        KGPU_zfar,   // far plane - must match camera!
                        m_render_config.clusters.tile_size,
                        m_render_config.clusters.depth_slices,
                        m_render_config.clusters.max_lights_per_cluster);

    const auto& grid_cfg = m_cluster_grid.get_config();
    m_cluster_config.tiles_x = grid_cfg.tiles_x;
    m_cluster_config.tiles_y = grid_cfg.tiles_y;
    m_cluster_config.depth_slices = grid_cfg.depth_slices;
    m_cluster_config.tile_size = grid_cfg.tile_size;
    m_cluster_config.near_plane = grid_cfg.near_plane;
    m_cluster_config.far_plane = grid_cfg.far_plane;
    m_cluster_config.log_depth_ratio = std::log(grid_cfg.far_plane / grid_cfg.near_plane);
    m_cluster_config.max_lights_per_cluster = grid_cfg.max_lights_per_cluster;
    m_cluster_config.screen_width = grid_cfg.screen_width;
    m_cluster_config.screen_height = grid_cfg.screen_height;

    // Initialize shadow passes, then register their depth views in bindless + create shaders
    init_shadow_passes();
    init_shadow_resources();

    // Initialize GPU compute shaders
    init_cluster_cull_compute();
    init_frustum_cull_compute();

    // Snapshot initial config for runtime change detection
    m_applied_clusters = m_render_config.clusters;
    m_applied_shadow_atlas_size = m_render_config.shadows.atlas_size;
    m_applied_shadow_csm_tile_size = m_render_config.shadows.csm_tile_size;
    m_applied_shadow_local_tile_size = m_render_config.shadows.local_tile_size;
    m_applied_shadow_depth_16bit = m_render_config.shadows.depth_16bit;

    // Setup and compile render graph — compile() validates all passes:
    // binding table resources + BDA push constant fields (bda_X → dyn_X).
    setup_render_graph();

    device.log_memory_stats();
}

void
vulkan_render::apply_config_changes()
{
    // Cluster config — reinit grid if any parameter changed
    const auto& c = m_render_config.clusters;
    if (c.tile_size != m_applied_clusters.tile_size ||
        c.depth_slices != m_applied_clusters.depth_slices ||
        c.max_lights_per_cluster != m_applied_clusters.max_lights_per_cluster)
    {
        m_cluster_grid.init(m_scene_lowres_width,
                            m_scene_lowres_height,
                            KGPU_znear,
                            KGPU_zfar,
                            c.tile_size,
                            c.depth_slices,
                            c.max_lights_per_cluster);
        m_clusters_dirty = true;
        m_applied_clusters = c;

        ALOG_INFO("Cluster grid reinitialized: tile_size={} depth_slices={} max_lights={}",
                  c.tile_size,
                  c.depth_slices,
                  c.max_lights_per_cluster);
    }

    // Shadow atlas config — requires recreating atlas + shader effects
    if (m_render_config.shadows.atlas_size != m_applied_shadow_atlas_size ||
        m_render_config.shadows.csm_tile_size != m_applied_shadow_csm_tile_size ||
        m_render_config.shadows.local_tile_size != m_applied_shadow_local_tile_size ||
        m_render_config.shadows.depth_16bit != m_applied_shadow_depth_16bit)
    {
        ALOG_INFO("Shadow atlas config changed: atlas {}→{}, csm {}→{}, local {}→{}",
                  m_applied_shadow_atlas_size,
                  m_render_config.shadows.atlas_size,
                  m_applied_shadow_csm_tile_size,
                  m_render_config.shadows.csm_tile_size,
                  m_applied_shadow_local_tile_size,
                  m_render_config.shadows.local_tile_size);

        glob::glob_state().getr_render().device.wait_for_fences();

        init_shadow_passes();
        init_shadow_resources();

        m_applied_shadow_atlas_size = m_render_config.shadows.atlas_size;
        m_applied_shadow_csm_tile_size = m_render_config.shadows.csm_tile_size;
        m_applied_shadow_local_tile_size = m_render_config.shadows.local_tile_size;
        m_applied_shadow_depth_16bit = m_render_config.shadows.depth_16bit;

        m_render_graph.reset();
        setup_render_graph();
    }
}

bool
vulkan_render::reconfigure_render_scale_live(uint32_t new_divisor)
{
    if (!m_render_config.render_scale.enabled)
    {
        ALOG_WARN("reconfigure_render_scale_live: render_scale.enabled = false — ignoring");
        return false;
    }
    new_divisor = std::clamp(new_divisor, 1u, 10u);
    if (new_divisor == m_render_config.render_scale.divisor)
    {
        return true;
    }

    auto& device = glob::glob_state().getr_render().device;
    vkDeviceWaitIdle(device.vk_device());

    const uint32_t new_w = std::max(1u, m_width / new_divisor);
    const uint32_t new_h = std::max(1u, m_height / new_divisor);

    // Build the new scene_lowres image that main pass renders into.
    auto swapchain_fmt = device.get_swapchain_format();
    VkExtent3D lowres_extent = {new_w, new_h, 1};
    auto img_info = vk_utils::make_image_create_info(
        swapchain_fmt,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        lowres_extent);
    VmaAllocationCreateInfo img_alloc = {};
    img_alloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto new_img = std::make_shared<vk_utils::vulkan_image>(
        vk_utils::vulkan_image::create(device.get_vma_allocator_provider(), img_info, img_alloc));
    auto new_view_ci = vk_utils::make_imageview_create_info(
        swapchain_fmt, new_img->image(), VK_IMAGE_ASPECT_COLOR_BIT);
    auto new_view = vk_utils::vulkan_image_view::create_shared(new_view_ci);

    // Swap into main pass — keeps VkRenderPass + SEs intact.
    auto* main_pass = glob::glob_state().getr_render().loader.get_render_pass(AID("main"));
    if (!main_pass)
    {
        return false;
    }

    // Drop scene_depth_texture's view+wrapper BEFORE replace_color_targets
    // destroys main's old depth image (see toggle path for full rationale).
    if (auto* dtex = m_cache.textures.find_by_id(AID("scene_depth_texture")))
    {
        dtex->image_view.reset();
        dtex->image.reset();
    }

    const bool outline_enabled = m_render_config.outline.enabled;
    if (!main_pass->replace_color_targets({new_img},
                                          {new_view},
                                          new_w,
                                          new_h,
                                          /*sampled_depth=*/true,
                                          /*enable_stencil=*/true,
                                          "main"))
    {
        return false;
    }

    // Replace the scene_upscale texture so the composite pass samples the new image.
    auto& loader = glob::glob_state().getr_render().loader;
    if (m_scene_upscale_txt)
    {
        loader.destroy_texture_data(AID("scene_lowres_txt"));
        m_scene_upscale_txt = nullptr;
    }
    m_scene_upscale_txt = loader.create_texture(AID("scene_lowres_txt"), new_img, new_view);
    if (m_scene_upscale_mat && m_scene_upscale_txt)
    {
        m_scene_upscale_mat->set_bindless_texture_index(0,
                                                        m_scene_upscale_txt->get_bindless_index());
        m_scene_upscale_mat->set_bindless_sampler_index(0, KGPU_SAMPLER_NEAREST_CLAMP);
    }

    // Whenever render_scale is on, downstream samples main-pass depth: outline
    // (when outline.enabled) and the grid (always — for occlusion). We
    // destroyed+recreated the depth image above, so the previous bindless slot
    // now points at a freed view. Update the existing texture_data in-place
    // (init_scene_depth created it directly via cache.textures.alloc, so we
    // can't use loader.destroy_texture_data here).
    if (m_render_config.render_scale.enabled && !main_pass->get_depth_images().empty())
    {
        auto& cache = get_cache();
        auto* dtex = cache.textures.find_by_id(AID("scene_depth_texture"));
        if (dtex)
        {
            auto& depth_img = main_pass->get_depth_images()[0];
            auto img_handle = depth_img.image();
            auto img_sptr = std::shared_ptr<vk_utils::vulkan_image>(new vk_utils::vulkan_image(
                vk_utils::vulkan_image::create(img_handle, VK_FORMAT_D32_SFLOAT_S8_UINT)));

            auto depth_view_ci = vk_utils::make_imageview_create_info(
                VK_FORMAT_D32_SFLOAT_S8_UINT, img_handle, VK_IMAGE_ASPECT_DEPTH_BIT);
            auto depth_view = vk_utils::vulkan_image_view::create_shared(depth_view_ci);

            dtex->image = std::move(img_sptr);
            dtex->image_view = std::move(depth_view);
            m_scene_depth_bindless_idx = dtex->get_bindless_index();
            stage_update_texture(dtex);
        }
    }

    // Bookkeeping
    m_scene_lowres_images = {new_img};
    m_scene_lowres_views = {new_view};
    m_scene_lowres_width = new_w;
    m_scene_lowres_height = new_h;
    m_render_config.render_scale.divisor = new_divisor;

    // Cluster grid tiles are sized in scene-pixels — must match the new
    // rendering resolution, otherwise GPU-side fragment-to-cluster mapping
    // diverges from CPU-side culling and produces lighting artifacts.
    m_cluster_grid.init(m_scene_lowres_width,
                        m_scene_lowres_height,
                        KGPU_znear,
                        KGPU_zfar,
                        m_render_config.clusters.tile_size,
                        m_render_config.clusters.depth_slices,
                        m_render_config.clusters.max_lights_per_cluster);
    m_clusters_dirty = true;

    ALOG_INFO(
        "render_scale live reconfig: divisor={} scene_lowres={}x{}", new_divisor, new_w, new_h);
    return true;
}

bool
vulkan_render::reconfigure_render_scale_enabled(bool enabled)
{
    if (enabled == m_render_config.render_scale.enabled)
    {
        return true;
    }

    auto& device = glob::glob_state().getr_render().device;
    vkDeviceWaitIdle(device.vk_device());

    auto& loader = glob::glob_state().getr_render().loader;

    // Tear down render-scale-dependent state. m_ui_copy_se's pipeline was
    // compiled against the old host pass and must be dropped regardless of
    // direction.
    m_scene_upscale_se = nullptr;
    m_scene_upscale_mat = nullptr;
    m_scene_upscale_txt = nullptr;
    m_depth_outline_se = nullptr;
    m_ui_copy_se = nullptr;
    m_ui_target_mat = nullptr;

    loader.destroy_material_data(AID("mat_ui_copy"));
    loader.destroy_material_data(AID("mat_scene_upscale"));
    loader.destroy_texture_data(AID("scene_lowres_txt"));

    // Drop scene_depth_texture's view+wrapper BEFORE replace_color_targets
    // destroys main's old depth image. Otherwise the view outlives its image
    // and the eventual vkDestroyImageView corrupts validation-layer state.
    if (auto* dtex = m_cache.textures.find_by_id(AID("scene_depth_texture")))
    {
        dtex->image_view.reset();
        dtex->image.reset();
    }

    if (m_render_config.render_scale.enabled)
    {
        // Currently on — tearing render_scale path down.
        loader.destroy_render_pass(AID("composite"));
        m_composite_pass.reset();
        m_scene_lowres_views.clear();
        m_scene_lowres_images.clear();
    }

    m_render_config.render_scale.enabled = enabled;

    auto* main_pass = loader.get_render_pass(AID("main"));
    KRG_check(main_pass, "main render pass missing");

    auto swapchain_fmt = device.get_swapchain_format();

    if (enabled)
    {
        const uint32_t scale = std::max(1u, m_render_config.render_scale.divisor);
        const uint32_t lw = std::max(1u, m_width / scale);
        const uint32_t lh = std::max(1u, m_height / scale);
        m_scene_lowres_width = lw;
        m_scene_lowres_height = lh;

        VkExtent3D extent = {lw, lh, 1};
        auto img_info = vk_utils::make_image_create_info(
            swapchain_fmt,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            extent);
        VmaAllocationCreateInfo img_alloc = {};
        img_alloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        auto img = std::make_shared<vk_utils::vulkan_image>(vk_utils::vulkan_image::create(
            device.get_vma_allocator_provider(), img_info, img_alloc));
        auto view_ci = vk_utils::make_imageview_create_info(
            swapchain_fmt, img->image(), VK_IMAGE_ASPECT_COLOR_BIT);
        auto view = vk_utils::vulkan_image_view::create_shared(view_ci);
        m_scene_lowres_images = {img};
        m_scene_lowres_views = {view};

        // Swap main pass to scene_lowres. replace_color_targets keeps
        // VkRenderPass identity, so SEs compiled against main survive.
        // sampled_depth=true: depth ends in READ_ONLY layout for composite to
        // sample (used by depth_outline AND the grid for occlusion).
        if (!main_pass->replace_color_targets(
                m_scene_lowres_images, m_scene_lowres_views, lw, lh, true, true, "main"))
        {
            return false;
        }

        auto composite_pass =
            render_pass_builder()
                .set_color_format(swapchain_fmt)
                .set_depth_format(VK_FORMAT_D32_SFLOAT_S8_UINT)
                .set_width_depth(m_width, m_height)
                .set_color_images(device.get_swapchain_image_views(), device.get_swapchain_images())
                .set_debug_name("composite")
                .build();
        loader.add_render_pass(AID("composite"), std::move(composite_pass));

        // Bindings — must mirror prepare_pass_bindings()'s composite section.
        auto* composite_for_bindings = loader.get_render_pass(AID("composite"));
        auto* layout_cache = device.descriptor_layout_cache();
        composite_for_bindings->bindings()
            .add(AID("static_samplers"),
                 KGPU_textures_descriptor_sets,
                 0,
                 VK_DESCRIPTOR_TYPE_SAMPLER,
                 VK_SHADER_STAGE_FRAGMENT_BIT,
                 render::binding_scope::per_material)
            .add(AID("bindless_textures"),
                 KGPU_textures_descriptor_sets,
                 1,
                 VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                 VK_SHADER_STAGE_FRAGMENT_BIT,
                 render::binding_scope::per_material);
        composite_for_bindings->finalize_bindings(*layout_cache);
    }
    else
    {
        m_scene_lowres_width = m_width;
        m_scene_lowres_height = m_height;
        // sampled_depth=true — main_pass's finalLayout is fixed at READ_ONLY
        // (sampled_depth=true at builder time); image must carry SAMPLED usage
        // so the layout transition is legal even when nobody currently samples.
        if (!main_pass->replace_color_targets(device.get_swapchain_images(),
                                              device.get_swapchain_image_views(),
                                              m_width,
                                              m_height,
                                              true,
                                              true,
                                              "main"))
        {
            return false;
        }
    }

    // Register / refresh scene_depth_texture bindless slot whenever render_scale
    // is on — main pass's depth image was recreated by replace_color_targets
    // and is sampled by composite (depth_outline + grid occlusion). Allocate
    // the texture entry on first transition (when initial config had
    // render_scale off, the init-path alloc was skipped).
    if (m_render_config.render_scale.enabled && !main_pass->get_depth_images().empty())
    {
        auto* dtex = m_cache.textures.find_by_id(AID("scene_depth_texture"));
        if (!dtex)
        {
            dtex = m_cache.textures.alloc(AID("scene_depth_texture"));
        }
        if (dtex)
        {
            auto& depth_img = main_pass->get_depth_images()[0];
            auto img_handle = depth_img.image();
            auto img_sptr = std::shared_ptr<vk_utils::vulkan_image>(new vk_utils::vulkan_image(
                vk_utils::vulkan_image::create(img_handle, VK_FORMAT_D32_SFLOAT_S8_UINT)));
            auto depth_view_ci = vk_utils::make_imageview_create_info(
                VK_FORMAT_D32_SFLOAT_S8_UINT, img_handle, VK_IMAGE_ASPECT_DEPTH_BIT);
            auto depth_view = vk_utils::vulkan_image_view::create_shared(depth_view_ci);
            dtex->image = std::move(img_sptr);
            dtex->image_view = std::move(depth_view);
            dtex->format = texture_format::unknown;
            m_scene_depth_bindless_idx = dtex->get_bindless_index();
            stage_update_texture(dtex);
        }
    }

    // Recreate ui_copy SE on the now-correct host pass. ui_copy lives on
    // main when render_scale is off, on composite when on — destroy from
    // BOTH possible owners so a stale entry from the previous state doesn't
    // collide with the new creation.
    if (auto* main_for_uicopy = loader.get_render_pass(AID("main")))
    {
        main_for_uicopy->destroy_shader_effect(AID("se_ui_copy"));
    }
    if (auto* composite_for_uicopy = loader.get_render_pass(AID("composite")))
    {
        composite_for_uicopy->destroy_shader_effect(AID("se_ui_copy"));
    }
    {
        vfs::rid se_ui_base("data://packages/base.apkg/class/shader_effects/ui");
        auto vert_r = render::shader_loader::load(se_ui_base / "se_upload.vert.spv");
        auto frag_r = render::shader_loader::load(se_ui_base / "se_upload.frag.spv");
        if (!vert_r || !frag_r)
        {
            ALOG_ERROR("Failed to load se_upload shaders — render-scale reconfigure aborted");
            return false;
        }
        auto& vert = *vert_r;
        auto& frag = *frag_r;

        auto host_pass_id = enabled ? AID("composite") : AID("main");
        auto host_pass = loader.get_render_pass(host_pass_id);
        KRG_check(host_pass, "host pass for se_ui_copy missing");

        shader_effect_create_info se_ci;
        se_ci.vert_buffer = &vert;
        se_ci.frag_buffer = &frag;
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = false;
        se_ci.alpha = alpha_mode::ui;
        se_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;
        se_ci.depth_write = false;

        host_pass->create_shader_effect(AID("se_ui_copy"), se_ci, m_ui_copy_se);

        std::vector<texture_sampler_data> samples(1);
        samples.front().texture = m_ui_target_txt;
        samples.front().slot = 0;

        m_ui_target_mat = loader.create_material(
            AID("mat_ui_copy"), AID("ui_copy"), samples, *m_ui_copy_se, utils::dynobj{});
    }

    if (enabled)
    {
        auto* composite_pass = loader.get_render_pass(AID("composite"));
        KRG_check(composite_pass, "composite pass should exist after enable");

        vfs::rid se_ui_base("data://packages/base.apkg/class/shader_effects/ui");
        vfs::rid se_base("data://packages/base.apkg/class/shader_effects");

        // Scene upscale
        {
            auto vert_r = render::shader_loader::load(se_ui_base / "se_upload.vert.spv");
            auto frag_r = render::shader_loader::load(se_ui_base / "se_upload.frag.spv");
            if (!vert_r || !frag_r)
            {
                ALOG_ERROR("Failed to load se_upload shaders for scene upscale");
                return false;
            }
            auto& vert = *vert_r;
            auto& frag = *frag_r;

            shader_effect_create_info se_ci;
            se_ci.vert_buffer = &vert;
            se_ci.frag_buffer = &frag;
            se_ci.is_wire = false;
            se_ci.enable_dynamic_state = false;
            se_ci.alpha = alpha_mode::ui;
            se_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;
            se_ci.depth_write = false;

            composite_pass->create_shader_effect(
                AID("se_scene_upscale"), se_ci, m_scene_upscale_se);

            m_scene_upscale_txt = loader.create_texture(
                AID("scene_lowres_txt"), m_scene_lowres_images[0], m_scene_lowres_views[0]);

            std::vector<texture_sampler_data> samples(1);
            samples.front().texture = m_scene_upscale_txt;
            samples.front().slot = 0;

            m_scene_upscale_mat = loader.create_material(AID("mat_scene_upscale"),
                                                         AID("scene_upscale"),
                                                         samples,
                                                         *m_scene_upscale_se,
                                                         utils::dynobj{});

            if (m_scene_upscale_mat)
            {
                m_scene_upscale_mat->set_bindless_sampler_index(0, KGPU_SAMPLER_NEAREST_CLAMP);
            }
        }

        // Depth outline (only when outline is also enabled)
        if (m_render_config.outline.enabled)
        {
            auto overt_r = render::shader_loader::load(se_base / "system/se_fullscreen.vert.spv");
            auto ofrag_r =
                render::shader_loader::load(se_base / "system/se_depth_outline.frag.spv");
            if (!overt_r || !ofrag_r)
            {
                ALOG_ERROR("Failed to load depth-outline shaders");
                return false;
            }
            auto& overt = *overt_r;
            auto& ofrag = *ofrag_r;

            shader_effect_create_info ose_ci;
            ose_ci.vert_buffer = &overt;
            ose_ci.frag_buffer = &ofrag;
            ose_ci.is_wire = false;
            ose_ci.enable_dynamic_state = false;
            ose_ci.alpha = alpha_mode::ui;
            ose_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;
            ose_ci.depth_write = false;

            composite_pass->create_shader_effect(
                AID("se_depth_outline"), ose_ci, m_depth_outline_se);
        }
    }

    // Cluster grid tiles are sized in scene-pixels — must match the new
    // rendering resolution after the toggle, otherwise GPU-side fragment-to-
    // cluster mapping diverges from CPU-side culling and produces lighting
    // artifacts at the new scale.
    m_cluster_grid.init(m_scene_lowres_width,
                        m_scene_lowres_height,
                        KGPU_znear,
                        KGPU_zfar,
                        m_render_config.clusters.tile_size,
                        m_render_config.clusters.depth_slices,
                        m_render_config.clusters.max_lights_per_cluster);
    m_clusters_dirty = true;

    m_render_graph.reset();
    setup_render_graph();

    ALOG_INFO("render_scale.enabled toggled: {}", enabled);
    return true;
}

void
vulkan_render::apply_pending_render_config()
{
    // Topology changes — must run reconfigure functions, which themselves
    // update the corresponding fields in m_render_config and do GPU work.
    if (m_pending_render_config.render_scale.enabled != m_render_config.render_scale.enabled)
    {
        reconfigure_render_scale_enabled(m_pending_render_config.render_scale.enabled);
    }

    // Live-reconfigurable: divisor (only meaningful when render_scale is enabled).
    if (m_render_config.render_scale.enabled &&
        m_pending_render_config.render_scale.divisor != m_render_config.render_scale.divisor)
    {
        reconfigure_render_scale_live(m_pending_render_config.render_scale.divisor);
    }

    // frames_in_flight and present mode are both swapchain-recreate triggers;
    // fold them into a single recreate so toggling both in one apply rebuilds
    // the swapchain once, not twice.
    const bool fif_changed =
        m_pending_render_config.frames_in_flight != m_render_config.frames_in_flight;
    const bool present_changed = m_pending_render_config.present != m_render_config.present;
    if (fif_changed || present_changed)
    {
        reconfigure_swapchain(m_pending_render_config.frames_in_flight,
                              m_pending_render_config.present);
        // reconfigure clamps the count to the actual swapchain image count (and
        // may bump it — mailbox needs >=3); mirror the results into pending so
        // the catch-all copy below doesn't reintroduce a mismatch (which would
        // re-trigger the reconfigure every frame).
        m_pending_render_config.frames_in_flight = m_render_config.frames_in_flight;
        m_pending_render_config.present = m_render_config.present;
    }

    // Catch-all: copy any remaining data-only fields (debug toggles, colors,
    // thresholds, etc.) from pending to active. Reconfigure functions above
    // already wrote the topology-affecting fields, so this is a no-op for
    // those.
    m_render_config = m_pending_render_config;

    // Consumed: the engine drained the render pipeline to idle before this call
    // (has_pending_render_config gates that), so clear the flag now that active
    // == pending again.
    m_render_config_dirty = false;
}

void
vulkan_render::reconfigure_swapchain(uint32_t count, present_mode mode)
{
    auto& device = glob::glob_state().getr_render().device;
    auto& loader = glob::glob_state().getr_render().loader;

    const uint32_t old_count = device.frames_in_flight();
    if (count == old_count && mode == device.current_present_mode())
    {
        m_render_config.frames_in_flight = old_count;
        m_render_config.present = mode;
        return;
    }

    const bool render_scale = m_render_config.render_scale.enabled;

    // Recreate the swapchain with `count` images. The device keeps the invariant
    // frames_in_flight == swapchain image count, so we never end up with more
    // images than CPU frame slots (the condition that produced garbage/red
    // present frames). The callback rebuilds the framebuffers of whichever pass
    // owns the swapchain as its color target — main when render_scale is off,
    // composite when on — keeping each pass's VkRenderPass identity (so all
    // shader effects compiled against it survive).
    const uint32_t new_count = device.recreate_swapchain(
        count,
        mode,
        [&](const std::vector<vk_utils::vulkan_image_sptr>& imgs,
            const std::vector<vk_utils::vulkan_image_view_sptr>& views)
        {
            if (render_scale)
            {
                auto* composite = loader.get_render_pass(AID("composite"));
                KRG_check(composite, "composite pass missing while render_scale enabled");
                composite->replace_color_targets(
                    imgs, views, m_width, m_height, false, true, "composite");
            }
            else
            {
                auto* main_pass = loader.get_render_pass(AID("main"));
                KRG_check(main_pass, "main pass missing");
                // sampled_depth=true / enable_stencil=true mirrors the main
                // pass's original swapchain-target configuration.
                main_pass->replace_color_targets(
                    imgs, views, m_width, m_height, true, true, "main");
            }
        });

    // Surviving slots [0, min(old,new)) keep their valid buffers untouched —
    // recreate_swapchain only rebuilt the swapchain-image framebuffers, not the
    // per-frame SSBOs we own here. We only touch the slots that actually change:
    // grow seeds the new ones from a survivor, shrink frees the dropped ones, a
    // pure present-mode switch (new == old) touches none. recreate_swapchain
    // already idled the device, so freeing/cloning buffers here is safe.
    KRG_check(old_count >= 1, "reconfigure_swapchain with no existing frame slot");

    if (new_count > old_count)
    {
        // Grow: create each new slot's buffers, then seed it from slot 0 (always
        // live). Cloning a survivor + replaying its pending queue reconstructs
        // the full scene without re-deriving from the model caches — so a new
        // renderable type can't silently miss the rebuild.
        for (uint32_t i = old_count; i < new_count; ++i)
        {
            create_frame_buffers(i);
            seed_frame_slot_from(i, 0);
        }
    }
    else if (new_count < old_count)
    {
        // Shrink: release the dropped slots' buffers to reclaim memory. No
        // re-seed needed — survivors are already correct.
        for (uint32_t i = new_count; i < old_count; ++i)
        {
            m_frames[i].buffers = frame_buffers{};
            m_frames[i].uploads.clear_all();
            m_frames[i].ui = ui_frame_state{};
        }
    }
    m_allocated_frame_slots = new_count;

    // Probes are a bulk replace gated by their own counter; re-arm it so every
    // live slot (including any just grown) re-uploads the current payload over
    // the next new_count frames. The upload is idempotent, so re-hitting already
    // correct survivors is harmless.
    m_probes_pending_uploads = new_count;

    m_render_config.frames_in_flight = new_count;
    m_render_config.present = device.current_present_mode();

    ALOG_INFO("swapchain reconfigured: {} -> {} images, present {}",
              old_count,
              new_count,
              to_string(m_render_config.present));
}

void
vulkan_render::deinit()
{
    // Wait for all GPU operations to complete before destroying resources
    vkDeviceWaitIdle(glob::glob_state().getr_render().device.vk_device());

    // Drop scene_depth_texture's image_view BEFORE the loader's clear_caches
    // destroys main_pass (and with it the underlying depth image). The view
    // is non-owning of the image but the destruction order still matters to
    // the validation layer's image-view tracking.
    if (auto* dtex = m_cache.textures.find_by_id(AID("scene_depth_texture")))
    {
        dtex->image_view.reset();
        dtex->image.reset();
    }

    // Render-scale / composite resources. Must release before VMA teardown
    // because scene_lowres images own VMA allocations.
    m_scene_upscale_se = nullptr;
    m_scene_upscale_mat = nullptr;
    m_scene_upscale_txt = nullptr;
    m_depth_outline_se = nullptr;
    m_ui_target_txt = nullptr;
    m_composite_pass.reset();
    m_scene_lowres_views.clear();
    m_scene_lowres_images.clear();

    // Clear shadow atlas
    m_shadow_se = nullptr;
    m_shadow_dpsm_se = nullptr;
    m_shadow_atlas_pass.reset();

    // Clear compute passes before device is destroyed
    // (they hold compute shaders with VkShaderModule)
    m_cluster_cull_shader = nullptr;
    m_cluster_cull_pass.reset();
    m_frustum_cull_shader = nullptr;
    m_frustum_cull_pass.reset();

    // Reset render graph so it can be recompiled on next init
    m_render_graph.reset();

    // Cleanup bindless textures
    deinit_bindless_textures();

    // Cleanup static samplers
    deinit_static_samplers();

    // Clear render queues (they hold raw pointers into pools)
    m_default_render_object_queue.clear();
    m_outline_render_object_queue.clear();
    m_transparent_render_object_queue.clear();
    m_debug_render_object_queue.clear();
    m_draw_batches.clear();
    m_debug_draw_batches.clear();
    m_instance_slots_staging.clear();
    m_materials_layout = {};

    // Clear all caches before VMA is destroyed
    m_cache.objects.clear();
    m_cache.directional_lights.clear();
    m_cache.universal_lights.clear();
    m_cache.textures.clear();

    m_global_textures_queue.clear();
    m_frames.clear();

    // Flush all deferred deletions now — vkDeviceWaitIdle above guarantees no
    // GPU work is in flight, so every scheduled resource is safe to destroy.
    glob::glob_state().getr_render().device.flush_deferred_deletions();
}

void
vulkan_render::prepare_system_resources()
{
    glob::glob_state().getr_render().loader.create_sampler(AID("default"),
                                                           VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);

    glob::glob_state().getr_render().loader.create_sampler(AID("font"),
                                                           VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);

    // Fullscreen quad used by grid, outline post-process, etc.
    {
        auto vl = gpu_dynobj_builder()
                      .add_field(AID("vPosition"), gpu_type::g_vec3, 1)
                      .add_field(AID("vNormal"), gpu_type::g_vec3, 1)
                      .add_field(AID("vColor"), gpu_type::g_vec3, 1)
                      .add_field(AID("vTexCoord"), gpu_type::g_vec2, 1)
                      .add_field(AID("vLightmapUV"), gpu_type::g_vec2, 1)
                      .finalize();

        auto val = gpu_dynobj_builder().add_array(AID("verts"), vl, 1, 4, 4).finalize();

        utils::buffer vert_buffer(val->get_object_size());
        {
            auto v = val->make_view<gpu_type>(vert_buffer.data());

            using v3 = glm::vec3;
            using v2 = glm::vec2;

            v.subobj(0, 0).write(v3{-1.f, 1.f, 0.f}, v3{0.f}, v3{0.f}, v2{0.f, 0.f}, v2{0.f, 0.f});
            v.subobj(0, 1).write(v3{1.f, 1.f, 0.f}, v3{0.f}, v3{0.f}, v2{1.f, 0.f}, v2{0.f, 0.f});
            v.subobj(0, 2).write(v3{-1.f, -1.f, 0.f}, v3{0.f}, v3{0.f}, v2{0.f, 1.f}, v2{0.f, 0.f});
            v.subobj(0, 3).write(v3{1.f, -1.f, 0.f}, v3{0.f}, v3{0.f}, v2{1.f, 1.f}, v2{0.f, 0.f});
        }

        utils::buffer index_buffer(6 * 4);
        auto v = index_buffer.make_view<uint32_t>();
        v.at(0) = 0;
        v.at(1) = 2;
        v.at(2) = 1;
        v.at(3) = 2;
        v.at(4) = 3;
        v.at(5) = 1;

        glob::glob_state().getr_render().loader.create_mesh(
            AID("plane_mesh"),
            vert_buffer.make_view<gpu::vertex_data>(),
            index_buffer.make_view<gpu::uint>());
    }

    kryga::utils::buffer vert, frag;

    vfs::rid se_base("data://packages/base.apkg/class/shader_effects");

    auto load_pair = [&vert, &frag](const vfs::rid& v_rid, const vfs::rid& f_rid) -> bool
    {
        auto v_r = render::shader_loader::load(v_rid);
        auto f_r = render::shader_loader::load(f_rid);
        if (!v_r || !f_r)
        {
            ALOG_ERROR("Failed to load shader pair: [{}] / [{}]", v_rid.str(), f_rid.str());
            return false;
        }
        vert = std::move(*v_r);
        frag = std::move(*f_r);
        return true;
    };

    if (!load_pair(se_base / "error/se_error.vert.spv", se_base / "error/se_error.frag.spv"))
    {
        return;
    }

    auto main_pass = glob::glob_state().getr_render().loader.get_render_pass(AID("main"));

    shader_effect_create_info se_ci;
    se_ci.vert_buffer = &vert;
    se_ci.frag_buffer = &frag;
    se_ci.is_wire = false;
    se_ci.enable_dynamic_state = false;
    se_ci.alpha = alpha_mode::none;
    se_ci.cull_mode = VK_CULL_MODE_NONE;
    se_ci.height = m_height;
    se_ci.width = m_width;

    shader_effect_data* sed = nullptr;
    auto rc = main_pass->create_shader_effect(AID("se_error"), se_ci, sed);
    KRG_check(rc == result_code::ok && sed, "Always should be good!");

    std::vector<texture_sampler_data> sd;

    // Grid shader effect and material
    if (!load_pair(se_base / "system/se_grid.vert.spv", se_base / "system/se_grid.frag.spv"))
    {
        return;
    }

    se_ci = {};
    se_ci.vert_buffer = &vert;
    se_ci.frag_buffer = &frag;
    se_ci.is_wire = false;
    se_ci.enable_dynamic_state = false;
    se_ci.alpha = alpha_mode::world;
    se_ci.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
    se_ci.cull_mode = VK_CULL_MODE_NONE;
    se_ci.ds_mode = depth_stencil_mode::none;
    se_ci.height = m_height;
    se_ci.width = m_width;

    m_grid_se = nullptr;
    rc = main_pass->create_shader_effect(AID("se_grid"), se_ci, m_grid_se);
    KRG_check(rc == result_code::ok && m_grid_se, "Grid shader effect creation failed!");

    m_grid_mat = glob::glob_state().getr_render().loader.create_material(
        AID("mat_grid"), AID("grid"), sd, *m_grid_se, utils::dynobj{});

    // Outline post-process shader — edge detection on selection mask
    {
        if (!load_pair(se_base / "system/se_fullscreen.vert.spv",
                       se_base / "system/se_outline_post.frag.spv"))
        {
            return;
        }

        se_ci = {};
        se_ci.vert_buffer = &vert;
        se_ci.frag_buffer = &frag;
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = false;
        se_ci.alpha = alpha_mode::world;
        se_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;
        se_ci.depth_write = false;
        se_ci.cull_mode = VK_CULL_MODE_NONE;
        se_ci.ds_mode = depth_stencil_mode::none;
        se_ci.height = m_height;
        se_ci.width = m_width;

        m_outline_post_se = nullptr;
        rc = main_pass->create_shader_effect(AID("se_outline_post"), se_ci, m_outline_post_se);
        KRG_check(rc == result_code::ok && m_outline_post_se, "Outline post SE failed!");

        m_outline_post_mat = glob::glob_state().getr_render().loader.create_material(
            AID("mat_outline_post"), AID("outline_post"), sd, *m_outline_post_se, utils::dynobj{});

        // Register the selection mask image in bindless so the post-process can sample it
        auto* sel_pass = get_render_pass(AID("selection_mask"));
        auto sel_images = sel_pass->get_color_images();
        if (!sel_images.empty())
        {
            auto& cache = glob::glob_state().getr_render().renderer.get_cache();
            auto* tex = cache.textures.alloc(AID("selection_mask_texture"));
            if (tex)
            {
                tex->image = sel_images[0];
                auto swapchain_fmt = glob::glob_state().getr_render().device.get_swapchain_format();
                auto view_ci = vk_utils::make_imageview_create_info(
                    swapchain_fmt, sel_images[0]->image(), VK_IMAGE_ASPECT_COLOR_BIT);
                tex->image_view = vk_utils::vulkan_image_view::create_shared(view_ci);
                tex->format = texture_format::rgba8;
                m_selection_mask_bindless_idx = tex->get_bindless_index();
                stage_update_texture(tex);
            }
        }
    }

    // Register main-pass depth in bindless whenever render_scale is on. Sampled
    // by depth_outline (when outline.enabled) and by the grid for occlusion.
    if (m_render_config.render_scale.enabled)
    {
        auto* main_pass_for_depth =
            glob::glob_state().getr_render().loader.get_render_pass(AID("main"));
        if (main_pass_for_depth && !main_pass_for_depth->get_depth_images().empty())
        {
            auto& depth_img = main_pass_for_depth->get_depth_images()[0];

            // Non-owning wrapper sptr — the render pass owns the actual image.
            auto img_handle = depth_img.image();
            auto img_sptr = std::shared_ptr<vk_utils::vulkan_image>(new vk_utils::vulkan_image(
                vk_utils::vulkan_image::create(img_handle, VK_FORMAT_D32_SFLOAT_S8_UINT)));

            auto depth_view_ci = vk_utils::make_imageview_create_info(
                VK_FORMAT_D32_SFLOAT_S8_UINT, img_handle, VK_IMAGE_ASPECT_DEPTH_BIT);
            auto depth_view = vk_utils::vulkan_image_view::create_shared(depth_view_ci);

            auto& cache = glob::glob_state().getr_render().renderer.get_cache();
            auto* dtex = cache.textures.alloc(AID("scene_depth_texture"));
            if (dtex)
            {
                dtex->image = std::move(img_sptr);
                dtex->image_view = std::move(depth_view);
                dtex->format = texture_format::unknown;
                m_scene_depth_bindless_idx = dtex->get_bindless_index();
                stage_update_texture(dtex);
            }
        }
    }

    // Depth-outline post-process — shader effect on the composite pass.
    if (m_render_config.render_scale.enabled && m_render_config.outline.enabled)
    {
        auto* composite_pass =
            glob::glob_state().getr_render().loader.get_render_pass(AID("composite"));

        if (composite_pass)
        {
            auto overt_r = render::shader_loader::load(se_base / "system/se_fullscreen.vert.spv");
            auto ofrag_r =
                render::shader_loader::load(se_base / "system/se_depth_outline.frag.spv");
            if (!overt_r || !ofrag_r)
            {
                ALOG_ERROR("Failed to load depth-outline composite shaders");
                return;
            }
            auto& overt = *overt_r;
            auto& ofrag = *ofrag_r;

            shader_effect_create_info ose_ci;
            ose_ci.vert_buffer = &overt;
            ose_ci.frag_buffer = &ofrag;
            ose_ci.is_wire = false;
            ose_ci.enable_dynamic_state = false;
            ose_ci.alpha = alpha_mode::ui;  // alpha blend over upscaled scene
            ose_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;
            ose_ci.depth_write = false;

            composite_pass->create_shader_effect(
                AID("se_depth_outline"), ose_ci, m_depth_outline_se);
        }
    }

    // Debug wireframe shader for light visualization
    if (!load_pair(se_base / "system/se_debug_wire.vert.spv",
                   se_base / "system/se_debug_wire.frag.spv"))
    {
        return;
    }

    se_ci = {};
    se_ci.vert_buffer = &vert;
    se_ci.frag_buffer = &frag;
    se_ci.is_wire = true;
    se_ci.enable_dynamic_state = false;
    se_ci.alpha = alpha_mode::none;
    se_ci.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
    se_ci.cull_mode = VK_CULL_MODE_NONE;
    se_ci.ds_mode = depth_stencil_mode::none;
    se_ci.height = m_height;
    se_ci.width = m_width;

    m_debug_wire_se = nullptr;
    rc = main_pass->create_shader_effect(AID("se_debug_wire"), se_ci, m_debug_wire_se);
    if (rc == result_code::ok && m_debug_wire_se)
    {
        m_debug_wire_mat = glob::glob_state().getr_render().loader.create_material(
            AID("mat_debug_wire"), AID("debug_wire"), sd, *m_debug_wire_se, utils::dynobj{});
    }

    // Create low-poly sphere mesh for debug point light radius
    {
        auto sphere = render::generate_sphere(1.0f, 12, 16, 1.0f, 1.0f, 0.0f);
        kryga::utils::buffer vb(sphere.vertices.size() * sizeof(gpu::vertex_data));
        std::memcpy(vb.data(), sphere.vertices.data(), vb.size());
        kryga::utils::buffer ib(sphere.indices.size() * sizeof(gpu::uint));
        std::memcpy(ib.data(), sphere.indices.data(), ib.size());
        m_debug_sphere_mesh = glob::glob_state().getr_render().loader.create_mesh(
            AID("debug_sphere_mesh"), vb.make_view<gpu::vertex_data>(), ib.make_view<gpu::uint>());
    }
    // Create cone mesh for debug spot light volume
    {
        auto cone = render::generate_cone(24, 1.0f, 1.0f, 0.0f);
        kryga::utils::buffer vb(cone.vertices.size() * sizeof(gpu::vertex_data));
        std::memcpy(vb.data(), cone.vertices.data(), vb.size());
        kryga::utils::buffer ib(cone.indices.size() * sizeof(gpu::uint));
        std::memcpy(ib.data(), cone.indices.data(), ib.size());
        m_debug_cone_mesh = glob::glob_state().getr_render().loader.create_mesh(
            AID("debug_cone_mesh"), vb.make_view<gpu::vertex_data>(), ib.make_view<gpu::uint>());
    }
}

void
vulkan_render::init_shadow_resources()
{
    ZoneScopedN("Render::InitShadowResources");

    // Register the single atlas depth image view in the bindless texture array.
    auto& device = glob::glob_state().getr_render().device;
    {
        auto depth_view = m_shadow_atlas_pass->get_depth_image_view(0);
        uint32_t bindless_idx = KGPU_max_bindless_textures - 1;

        VkDescriptorImageInfo image_info = {};
        image_info.imageView = depth_view;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_bindless_set;
        write.dstBinding = 1;
        write.dstArrayElement = bindless_idx;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo = &image_info;

        vkUpdateDescriptorSets(device.vk_device(), 1, &write, 0, nullptr);
        m_shadow_atlas_bindless_index = bindless_idx;
    }

    // Create shadow shader effects on the atlas pass
    vfs::rid se_base("data://packages/base.apkg/class/shader_effects");

    auto vert_r = render::shader_loader::load(se_base / "shadow/se_shadow.vert.spv");
    if (vert_r)
    {
        auto& vert = *vert_r;
        shader_effect_create_info se_ci = {};
        se_ci.vert_buffer = &vert;
        se_ci.frag_buffer = nullptr;
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = false;
        se_ci.alpha = alpha_mode::none;
        se_ci.cull_mode = VK_CULL_MODE_FRONT_BIT;
        se_ci.depth_bias_enable = true;
        se_ci.depth_bias_constant = 2.0f;
        se_ci.depth_bias_slope = 3.0f;
        se_ci.depth_bias_clamp = 0.0f;
        se_ci.height = m_render_config.shadows.atlas_size;
        se_ci.width = m_render_config.shadows.atlas_size;
        se_ci.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;

        m_shadow_se = nullptr;
        auto rc = m_shadow_atlas_pass->create_shader_effect(AID("se_shadow"), se_ci, m_shadow_se);
        if (rc != result_code::ok)
        {
            ALOG_WARN("Failed to create shadow shader effect - shadows disabled");
            m_shadow_se = nullptr;
        }
    }
    else
    {
        ALOG_WARN("Failed to load se_shadow.vert - shadows disabled");
    }

    auto dpsm_vert_r = render::shader_loader::load(se_base / "shadow/se_shadow_dpsm.vert.spv");
    if (dpsm_vert_r)
    {
        auto& dpsm_vert = *dpsm_vert_r;
        shader_effect_create_info se_ci = {};
        se_ci.vert_buffer = &dpsm_vert;
        se_ci.frag_buffer = nullptr;
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = false;
        se_ci.alpha = alpha_mode::none;
        se_ci.cull_mode = VK_CULL_MODE_NONE;
        se_ci.depth_bias_enable = true;
        se_ci.depth_bias_constant = 2.0f;
        se_ci.depth_bias_slope = 3.0f;
        se_ci.depth_bias_clamp = 0.0f;
        se_ci.height = m_render_config.shadows.atlas_size;
        se_ci.width = m_render_config.shadows.atlas_size;
        se_ci.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;

        m_shadow_dpsm_se = nullptr;
        auto rc = m_shadow_atlas_pass->create_shader_effect(
            AID("se_shadow_dpsm"), se_ci, m_shadow_dpsm_se);
        if (rc != result_code::ok)
        {
            ALOG_WARN("Failed to create DPSM shadow shader effect");
            m_shadow_dpsm_se = nullptr;
        }
    }

    KRG_check(m_shadow_se, "Shadow shader effect must be loaded");
    m_shadow_config.directional.cascade_count = m_render_config.shadows.cascade_count;
    m_shadow_config.directional.shadow_bias = m_render_config.shadows.bias;
    m_shadow_config.directional.normal_bias = m_render_config.shadows.normal_bias;
    m_shadow_config.directional.texel_size =
        1.0f / static_cast<float>(m_render_config.shadows.csm_tile_size);
    m_shadow_config.directional.pcf_mode = static_cast<uint32_t>(m_render_config.shadows.pcf);
    m_shadow_config.directional.pcf_world_radius = m_render_config.shadows.pcf_world_radius;
    m_shadow_config.directional.hardware_pcf = m_render_config.shadows.hardware_pcf ? 1u : 0u;
    m_shadow_config.shadowed_local_count = 0;
    m_shadow_config.atlas_bindless_index = m_shadow_atlas_bindless_index;

    // Transition atlas depth images to SHADER_READ_ONLY_OPTIMAL
    device.immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            auto& depth_images = m_shadow_atlas_pass->get_depth_images();
            for (auto& img : depth_images)
            {
                barrier.image = img.image();
                vkCmdPipelineBarrier(cmd,
                                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     1,
                                     &barrier);
            }
        });

    ALOG_INFO("Shadow atlas initialized: {}x{}, CSM tiles {}x{}, local tiles {}x{}, {} frames",
              m_render_config.shadows.atlas_size,
              m_render_config.shadows.atlas_size,
              m_render_config.shadows.csm_tile_size,
              m_render_config.shadows.csm_tile_size,
              m_render_config.shadows.local_tile_size,
              m_render_config.shadows.local_tile_size,
              1);
}

render_cache&
vulkan_render::get_cache()
{
    return m_cache;
}

render_pass*
vulkan_render::get_render_pass(const utils::id& id)
{
    return glob::glob_state().getr_render().loader.get_render_pass(id);
}

void
vulkan_render::init_static_samplers()
{
    auto vk_device = glob::glob_state().getr_render().device.vk_device();

    // Helper to create a sampler with given parameters
    auto create_sampler = [vk_device](
                              VkFilter filter,
                              VkSamplerAddressMode addressMode,
                              VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                              bool anisotropy = false) -> VkSampler
    {
        VkSamplerCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        ci.magFilter = filter;
        ci.minFilter = filter;
        ci.mipmapMode = (filter == VK_FILTER_LINEAR) ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                                                     : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        ci.addressModeU = addressMode;
        ci.addressModeV = addressMode;
        ci.addressModeW = addressMode;
        ci.mipLodBias = 0.0f;
        ci.anisotropyEnable = anisotropy ? VK_TRUE : VK_FALSE;
        ci.maxAnisotropy = anisotropy ? 16.0f : 1.0f;
        ci.compareEnable = VK_FALSE;
        ci.compareOp = VK_COMPARE_OP_ALWAYS;
        ci.minLod = 0.0f;
        ci.maxLod = VK_LOD_CLAMP_NONE;
        ci.borderColor = borderColor;
        ci.unnormalizedCoordinates = VK_FALSE;

        VkSampler sampler;
        VK_CHECK(vkCreateSampler(vk_device, &ci, nullptr, &sampler));
        return sampler;
    };

    // KGPU_SAMPLER_LINEAR_REPEAT (0) - Default for most textures
    m_static_samplers[KGPU_SAMPLER_LINEAR_REPEAT] =
        create_sampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

    // KGPU_SAMPLER_LINEAR_CLAMP (1) - Skyboxes, LUTs
    m_static_samplers[KGPU_SAMPLER_LINEAR_CLAMP] =
        create_sampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    // KGPU_SAMPLER_LINEAR_MIRROR (2) - Seamless tiling
    m_static_samplers[KGPU_SAMPLER_LINEAR_MIRROR] =
        create_sampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT);

    // KGPU_SAMPLER_NEAREST_REPEAT (3) - Pixel art, data textures
    m_static_samplers[KGPU_SAMPLER_NEAREST_REPEAT] =
        create_sampler(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);

    // KGPU_SAMPLER_NEAREST_CLAMP (4) - UI, fonts
    m_static_samplers[KGPU_SAMPLER_NEAREST_CLAMP] =
        create_sampler(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    // KGPU_SAMPLER_LINEAR_CLAMP_BORDER (5) - Shadow maps
    m_static_samplers[KGPU_SAMPLER_LINEAR_CLAMP_BORDER] =
        create_sampler(VK_FILTER_LINEAR,
                       VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                       VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);

    // KGPU_SAMPLER_ANISO_REPEAT (6) - High-quality surfaces
    // Note: Anisotropy requires the feature to be enabled at device creation.
    // For now, fall back to linear filtering. To enable anisotropy, add
    // samplerAnisotropy to device features in vulkan_render_device.cpp
    m_static_samplers[KGPU_SAMPLER_ANISO_REPEAT] =
        create_sampler(VK_FILTER_LINEAR,
                       VK_SAMPLER_ADDRESS_MODE_REPEAT,
                       VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                       false);

    // KGPU_SAMPLER_SHADOW_CMP (7) - Shadow map comparison sampler
    // Hardware PCF: depth test + bilinear interpolation in one tap.
    {
        VkSamplerCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        ci.magFilter = VK_FILTER_LINEAR;
        ci.minFilter = VK_FILTER_LINEAR;
        ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        ci.compareEnable = VK_TRUE;
        ci.compareOp = VK_COMPARE_OP_LESS;
        ci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        ci.minLod = 0.0f;
        ci.maxLod = VK_LOD_CLAMP_NONE;
        VK_CHECK(
            vkCreateSampler(vk_device, &ci, nullptr, &m_static_samplers[KGPU_SAMPLER_SHADOW_CMP]));
    }

    static const char* sampler_names[KGPU_SAMPLER_COUNT] = {"sampler.linear_repeat",
                                                            "sampler.linear_clamp",
                                                            "sampler.linear_mirror",
                                                            "sampler.nearest_repeat",
                                                            "sampler.nearest_clamp",
                                                            "sampler.linear_clamp_border",
                                                            "sampler.aniso_repeat",
                                                            "sampler.shadow_cmp"};
    for (int i = 0; i < KGPU_SAMPLER_COUNT; ++i)
    {
        if (m_static_samplers[i] != VK_NULL_HANDLE)
        {
            KRG_VK_NAME(vk_device, m_static_samplers[i], sampler_names[i]);
        }
    }

    ALOG_INFO("Static samplers initialized ({} variants)", KGPU_SAMPLER_COUNT);
}

void
vulkan_render::deinit_static_samplers()
{
    auto vk_device = glob::glob_state().getr_render().device.vk_device();

    for (int i = 0; i < KGPU_SAMPLER_COUNT; ++i)
    {
        if (m_static_samplers[i] != VK_NULL_HANDLE)
        {
            vkDestroySampler(vk_device, m_static_samplers[i], nullptr);
            m_static_samplers[i] = VK_NULL_HANDLE;
        }
    }
}

void
vulkan_render::init_bindless_textures()
{
    auto& device = glob::glob_state().getr_render().device;
    auto vk_device = device.vk_device();

    // Create descriptor pool with UPDATE_AFTER_BIND flag
    // Pool needs space for SAMPLED_IMAGE (textures) and SAMPLER (static samplers)
    VkDescriptorPoolSize pool_sizes[2]{};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    pool_sizes[0].descriptorCount = KGPU_max_bindless_textures;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    pool_sizes[1].descriptorCount = KGPU_SAMPLER_COUNT;

    VkDescriptorPoolCreateInfo pool_ci{};
    pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_ci.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool_ci.maxSets = 1;
    pool_ci.poolSizeCount = 2;
    pool_ci.pPoolSizes = pool_sizes;

    VK_CHECK(vkCreateDescriptorPool(vk_device, &pool_ci, nullptr, &m_bindless_pool));
    KRG_VK_NAME(vk_device, m_bindless_pool, "bindless.pool");

    // Create descriptor set layout with two bindings:
    // Binding 0: sampler static_samplers[KGPU_SAMPLER_COUNT] (SAMPLER, immutable)
    // Binding 1: texture2D bindless_textures[] (SAMPLED_IMAGE, variable count)
    // Note: Variable count flag must be on highest binding number
    VkDescriptorSetLayoutBinding bindings[2]{};

    // Binding 0: Static samplers (immutable)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[0].descriptorCount = KGPU_SAMPLER_COUNT;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = m_static_samplers;  // Immutable samplers

    // Binding 1: Bindless textures (SAMPLED_IMAGE, variable count)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[1].descriptorCount = KGPU_max_bindless_textures;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    // Binding flags: sampler has none, texture binding has variable count (must be last)
    VkDescriptorBindingFlags binding_flags[2]{};
    binding_flags[0] = 0;  // No special flags for immutable samplers
    binding_flags[1] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                       VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                       VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_ci{};
    binding_flags_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    binding_flags_ci.bindingCount = 2;
    binding_flags_ci.pBindingFlags = binding_flags;

    VkDescriptorSetLayoutCreateInfo layout_ci{};
    layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_ci.pNext = &binding_flags_ci;
    layout_ci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layout_ci.bindingCount = 2;
    layout_ci.pBindings = bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(vk_device, &layout_ci, nullptr, &m_bindless_layout));
    KRG_VK_NAME(vk_device, m_bindless_layout, "bindless.dsl");

    // Allocate the single bindless descriptor set
    // Variable count only applies to binding 0 (the last binding with variable count flag)
    uint32_t variable_count = KGPU_max_bindless_textures;

    VkDescriptorSetVariableDescriptorCountAllocateInfo variable_count_ai{};
    variable_count_ai.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variable_count_ai.descriptorSetCount = 1;
    variable_count_ai.pDescriptorCounts = &variable_count;

    VkDescriptorSetAllocateInfo set_ai{};
    set_ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_ai.pNext = &variable_count_ai;
    set_ai.descriptorPool = m_bindless_pool;
    set_ai.descriptorSetCount = 1;
    set_ai.pSetLayouts = &m_bindless_layout;

    VK_CHECK(vkAllocateDescriptorSets(vk_device, &set_ai, &m_bindless_set));
    KRG_VK_NAME(vk_device, m_bindless_set, "bindless.set");

    ALOG_INFO("Bindless textures initialized with {} max textures and {} static samplers",
              KGPU_max_bindless_textures,
              KGPU_SAMPLER_COUNT);
}

void
vulkan_render::deinit_bindless_textures()
{
    auto vk_device = glob::glob_state().getr_render().device.vk_device();

    if (m_bindless_layout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(vk_device, m_bindless_layout, nullptr);
        m_bindless_layout = VK_NULL_HANDLE;
    }

    if (m_bindless_pool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(vk_device, m_bindless_pool, nullptr);
        m_bindless_pool = VK_NULL_HANDLE;
    }

    m_bindless_set = VK_NULL_HANDLE;
}

}  // namespace render
}  // namespace kryga
