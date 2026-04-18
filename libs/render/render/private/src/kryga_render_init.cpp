#include "vulkan_render/kryga_render.h"

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/vulkan_render_loader_create_infos.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/utils/vulkan_debug.h"

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

#include <tracy/Tracy.hpp>

#include <cmath>
#include <format>
#include <string>

namespace kryga
{
void
state_mutator__vulkan_render::set(gs::state& s)
{
    auto p = s.create_box<render::vulkan_render>("vulkan_render");
    s.m_vulkan_render = p;
}

namespace render
{
namespace
{

const uint32_t INITIAL_MATERIAL_SEGMENT_RANGE_SIZE = 1024;
const uint32_t INITIAL_MATERIAL_RANGE_SIZE = 10 * INITIAL_MATERIAL_SEGMENT_RANGE_SIZE;

const uint32_t OBJECTS_BUFFER_SIZE = 16 * 1024;
const uint32_t UNIVERSAL_LIGHTS_BUFFER_SIZE = 1024;
const uint32_t DIRECT_LIGHTS_BUFFER_SIZE = 512;

const uint32_t DYNAMIC_BUFFER_SIZE = 1024;

}  // namespace

vulkan_render::vulkan_render()
{
}

vulkan_render::~vulkan_render()
{
}

void
vulkan_render::init(uint32_t w, uint32_t h, const render_config& config, bool only_rp)
{
    m_width = w;
    m_height = h;
    m_render_config = config;

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

    auto& device = glob::glob_state().getr_render_device();

    m_frames.resize(device.frame_size());

    for (size_t i = 0; i < m_frames.size(); ++i)
    {
        m_frames[i].frame = &device.frame(i);

        m_frames[i].buffers.objects =
            device.create_buffer(OBJECTS_BUFFER_SIZE,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU,
                                 0,
                                 KRG_VK_FMT_NAME("frame_{}.objects", i));

        m_frames[i].buffers.materials =
            device.create_buffer(INITIAL_MATERIAL_RANGE_SIZE,
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
            device.create_buffer(DYNAMIC_BUFFER_SIZE * DYNAMIC_BUFFER_SIZE * 24,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU,
                                 0,
                                 KRG_VK_FMT_NAME("frame_{}.dynamic_data", i));

        // Cluster buffers - used by both CPU upload and GPU compute
        // CPU_TO_GPU allows CPU writes for fallback, GPU can read/write via SSBO
        m_frames[i].buffers.cluster_counts =
            device.create_buffer(DYNAMIC_BUFFER_SIZE * DYNAMIC_BUFFER_SIZE * 24,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU,
                                 0,
                                 KRG_VK_FMT_NAME("frame_{}.cluster_counts", i));

        m_frames[i].buffers.cluster_indices =
            device.create_buffer(DYNAMIC_BUFFER_SIZE * DYNAMIC_BUFFER_SIZE * 24,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU,
                                 0,
                                 KRG_VK_FMT_NAME("frame_{}.cluster_indices", i));

        m_frames[i].buffers.cluster_config = device.create_buffer(
            DYNAMIC_BUFFER_SIZE * DYNAMIC_BUFFER_SIZE * 24,
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

    prepare_system_resources();

    // Skip UI setup in headless mode (no ImGui context)
    if (!device.is_headless())
    {
        prepare_ui_resources();
        prepare_ui_pipeline();
    }

    // Initialize clustered lighting (must match camera near/far planes)
    m_cluster_grid.init(m_width,
                        m_height,
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
    m_applied_shadow_map_size = m_render_config.shadows.map_size;

    // Setup and compile render graph — compile() validates all passes:
    // binding table resources + BDA push constant fields (bda_X → dyn_X).
    setup_render_graph();
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
        m_cluster_grid.init(m_width,
                            m_height,
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

    // Shadow map resolution — requires recreating shadow passes + shader effects
    if (m_render_config.shadows.map_size != m_applied_shadow_map_size)
    {
        ALOG_INFO("Shadow map resolution changed: {} -> {}",
                  m_applied_shadow_map_size,
                  m_render_config.shadows.map_size);

        glob::glob_state().getr_render_device().wait_for_fences();

        // Recreate shadow passes (new image dimensions) and resources (shader effects + bindless)
        init_shadow_passes();
        init_shadow_resources();

        m_applied_shadow_map_size = m_render_config.shadows.map_size;

        // Render graph references shadow passes — must rebuild
        m_render_graph.reset();
        setup_render_graph();
    }
}

void
vulkan_render::deinit()
{
    // Wait for all GPU operations to complete before destroying resources
    vkDeviceWaitIdle(glob::glob_state().get_render_device()->vk_device());

    // Clear shadow passes
    m_shadow_se = nullptr;
    m_shadow_dpsm_se = nullptr;
    for (auto& sp : m_shadow_passes)
    {
        sp.reset();
    }
    for (auto& sp : m_shadow_local_passes)
    {
        sp.reset();
    }

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
}

void
vulkan_render::prepare_system_resources()
{
    glob::glob_state().getr_vulkan_render_loader().create_sampler(
        AID("default"), VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);

    glob::glob_state().getr_vulkan_render_loader().create_sampler(
        AID("font"), VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);

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

        glob::glob_state().getr_vulkan_render_loader().create_mesh(
            AID("plane_mesh"),
            vert_buffer.make_view<gpu::vertex_data>(),
            index_buffer.make_view<gpu::uint>());
    }

    kryga::utils::buffer vert, frag;

    vfs::rid se_base("data://packages/base.apkg/class/shader_effects");

    vfs::load_buffer(se_base / "error/se_error.vert", vert);
    vfs::load_buffer(se_base / "error/se_error.frag", frag);

    auto main_pass = glob::glob_state().getr_vulkan_render_loader().get_render_pass(AID("main"));

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
    vfs::load_buffer(se_base / "system/se_grid.vert", vert);
    vfs::load_buffer(se_base / "system/se_grid.frag", frag);

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

    m_grid_mat = glob::glob_state().getr_vulkan_render_loader().create_material(
        AID("mat_grid"), AID("grid"), sd, *m_grid_se, utils::dynobj{});

    // Outline post-process shader — edge detection on selection mask
    {
        vfs::load_buffer(se_base / "system/se_outline_post.vert", vert);
        vfs::load_buffer(se_base / "system/se_outline_post.frag", frag);

        se_ci = {};
        se_ci.vert_buffer = &vert;
        se_ci.frag_buffer = &frag;
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = false;
        se_ci.alpha = alpha_mode::world;
        se_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;
        se_ci.cull_mode = VK_CULL_MODE_NONE;
        se_ci.ds_mode = depth_stencil_mode::none;
        se_ci.height = m_height;
        se_ci.width = m_width;

        m_outline_post_se = nullptr;
        rc = main_pass->create_shader_effect(AID("se_outline_post"), se_ci, m_outline_post_se);
        KRG_check(rc == result_code::ok && m_outline_post_se, "Outline post SE failed!");

        m_outline_post_mat = glob::glob_state().getr_vulkan_render_loader().create_material(
            AID("mat_outline_post"), AID("outline_post"), sd, *m_outline_post_se, utils::dynobj{});

        // Register the selection mask image in bindless so the post-process can sample it
        auto* sel_pass = get_render_pass(AID("selection_mask"));
        auto sel_images = sel_pass->get_color_images();
        if (!sel_images.empty())
        {
            auto& cache = glob::glob_state().getr_vulkan_render().get_cache();
            auto* tex = cache.textures.alloc(AID("selection_mask_texture"));
            if (tex)
            {
                tex->image = sel_images[0];
                auto swapchain_fmt = glob::glob_state().getr_render_device().get_swapchain_format();
                auto view_ci = vk_utils::make_imageview_create_info(
                    swapchain_fmt, sel_images[0]->image(), VK_IMAGE_ASPECT_COLOR_BIT);
                tex->image_view = vk_utils::vulkan_image_view::create_shared(view_ci);
                tex->format = texture_format::rgba8;
                m_selection_mask_bindless_idx = tex->get_bindless_index();
                schd_update_texture(tex);
            }
        }
    }

    // Debug wireframe shader for light visualization
    vfs::load_buffer(se_base / "system/se_debug_wire.vert", vert);
    vfs::load_buffer(se_base / "system/se_debug_wire.frag", frag);

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
        m_debug_wire_mat = glob::glob_state().getr_vulkan_render_loader().create_material(
            AID("mat_debug_wire"), AID("debug_wire"), sd, *m_debug_wire_se, utils::dynobj{});
    }
}

void
vulkan_render::init_shadow_resources()
{
    ZoneScopedN("Render::InitShadowResources");

    // Register CSM depth image views in bindless texture array
    for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
    {
        auto& device = glob::glob_state().getr_render_device();
        for (uint32_t f = 0; f < FRAMES_IN_FLIGHT; ++f)
        {
            auto depth_view = m_shadow_passes[c]->get_depth_image_view(f);

            uint32_t bindless_idx = KGPU_max_bindless_textures - 1 - (c * FRAMES_IN_FLIGHT + f);

            VkDescriptorImageInfo image_info = {};
            image_info.imageView = depth_view;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write = {};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_bindless_set;
            write.dstBinding = 1;  // bindless_textures binding
            write.dstArrayElement = bindless_idx;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            write.pImageInfo = &image_info;

            vkUpdateDescriptorSets(device.vk_device(), 1, &write, 0, nullptr);

            m_shadow_map_bindless_indices[c][f] = bindless_idx;
        }
        m_shadow_config.directional.shadow_map_indices[c] = m_shadow_map_bindless_indices[c][0];
    }

    // Register local light depth views in bindless array
    constexpr uint32_t csm_bindless_count = KGPU_CSM_CASCADE_COUNT * FRAMES_IN_FLIGHT;
    for (uint32_t i = 0; i < KGPU_MAX_SHADOWED_LOCAL_LIGHTS * 2; ++i)
    {
        auto vk_dev = glob::glob_state().getr_render_device().vk_device();
        for (uint32_t f = 0; f < FRAMES_IN_FLIGHT; ++f)
        {
            auto local_depth_view = m_shadow_local_passes[i]->get_depth_image_view(f);
            uint32_t local_bindless_idx =
                KGPU_max_bindless_textures - 1 - csm_bindless_count - (i * FRAMES_IN_FLIGHT + f);

            VkDescriptorImageInfo local_image_info = {};
            local_image_info.imageView = local_depth_view;
            local_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet local_write = {};
            local_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            local_write.dstSet = m_bindless_set;
            local_write.dstBinding = 1;
            local_write.dstArrayElement = local_bindless_idx;
            local_write.descriptorCount = 1;
            local_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            local_write.pImageInfo = &local_image_info;

            vkUpdateDescriptorSets(vk_dev, 1, &local_write, 0, nullptr);

            m_shadow_local_bindless_indices[i][f] = local_bindless_idx;
        }
    }

    // Create shadow vertex shader effect on the first shadow pass.
    // With BDA, no shared pipeline layout needed — shadow shaders build their own.
    vfs::rid se_base("data://packages/base.apkg/class/shader_effects");

    kryga::utils::buffer vert;
    if (vfs::load_buffer(se_base / "shadow/se_shadow.vert", vert))
    {
        shader_effect_create_info se_ci = {};
        se_ci.vert_buffer = &vert;
        se_ci.frag_buffer = nullptr;  // No fragment shader for depth-only
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = false;
        se_ci.alpha = alpha_mode::none;
        se_ci.cull_mode = VK_CULL_MODE_FRONT_BIT;  // Front-face culling reduces peter-panning
        se_ci.height = m_render_config.shadows.map_size;
        se_ci.width = m_render_config.shadows.map_size;
        se_ci.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;

        m_shadow_se = nullptr;
        auto rc = m_shadow_passes[0]->create_shader_effect(AID("se_shadow"), se_ci, m_shadow_se);
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

    // Create DPSM vertex shader for point lights
    kryga::utils::buffer dpsm_vert;
    if (vfs::load_buffer(se_base / "shadow/se_shadow_dpsm.vert", dpsm_vert))
    {
        shader_effect_create_info se_ci = {};
        se_ci.vert_buffer = &dpsm_vert;
        se_ci.frag_buffer = nullptr;
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = false;
        se_ci.alpha = alpha_mode::none;
        se_ci.cull_mode = VK_CULL_MODE_NONE;  // Can't cull in paraboloid space
        se_ci.height = m_render_config.shadows.map_size;
        se_ci.width = m_render_config.shadows.map_size;
        se_ci.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;

        m_shadow_dpsm_se = nullptr;
        auto rc = m_shadow_passes[0]->create_shader_effect(
            AID("se_shadow_dpsm"), se_ci, m_shadow_dpsm_se);
        if (rc != result_code::ok)
        {
            ALOG_WARN("Failed to create DPSM shadow shader effect");
            m_shadow_dpsm_se = nullptr;
        }
    }

    // Initialize shadow config from render_config
    m_shadow_config.directional.cascade_count = m_render_config.shadows.cascade_count;
    m_shadow_config.directional.shadow_bias = m_render_config.shadows.bias;
    m_shadow_config.directional.normal_bias = m_render_config.shadows.normal_bias;
    m_shadow_config.directional.texel_size =
        1.0f / static_cast<float>(m_render_config.shadows.map_size);
    m_shadow_config.directional.pcf_mode = static_cast<uint32_t>(m_render_config.shadows.pcf);
    m_shadow_config.shadowed_local_count = 0;

    // Transition all shadow depth images from UNDEFINED to SHADER_READ_ONLY_OPTIMAL.
    // This ensures unused shadow maps (inactive lights) are in a valid layout for sampling.
    // Active shadow maps will be transitioned by the render graph as needed.
    auto& device = glob::glob_state().getr_render_device();
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

            // CSM cascades
            for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
            {
                auto& depth_images = m_shadow_passes[c]->get_depth_images();
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
            }

            // Local light shadow passes
            for (uint32_t i = 0; i < KGPU_MAX_SHADOWED_LOCAL_LIGHTS * 2; ++i)
            {
                auto& depth_images = m_shadow_local_passes[i]->get_depth_images();
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
            }
        });

    ALOG_INFO("Shadow resources initialized: {} CSM cascades, {}x{} resolution, {} frames buffered",
              KGPU_CSM_CASCADE_COUNT,
              m_render_config.shadows.map_size,
              m_render_config.shadows.map_size,
              FRAMES_IN_FLIGHT);
}

render_cache&
vulkan_render::get_cache()
{
    return m_cache;
}

render_pass*
vulkan_render::get_render_pass(const utils::id& id)
{
    return glob::glob_state().getr_vulkan_render_loader().get_render_pass(id);
}

void
vulkan_render::init_static_samplers()
{
    auto vk_device = glob::glob_state().get_render_device()->vk_device();

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

    static const char* sampler_names[KGPU_SAMPLER_COUNT] = {"sampler.linear_repeat",
                                                            "sampler.linear_clamp",
                                                            "sampler.linear_mirror",
                                                            "sampler.nearest_repeat",
                                                            "sampler.nearest_clamp",
                                                            "sampler.linear_clamp_border",
                                                            "sampler.aniso_repeat"};
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
    auto vk_device = glob::glob_state().get_render_device()->vk_device();

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
    auto device = glob::glob_state().get_render_device();
    auto vk_device = device->vk_device();

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
    auto vk_device = glob::glob_state().get_render_device()->vk_device();

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
