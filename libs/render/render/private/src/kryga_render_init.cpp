#include "vulkan_render/kryga_render.h"
#include "vulkan_render/vulkan_loaders/vulkan_compute_shader_loader.h"

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/types/vulkan_render_pass_builder.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"

#include <gpu_types/gpu_generic_constants.h>
#include <gpu_types/gpu_frustum_types.h>
#include <gpu_types/gpu_shadow_types.h>

#include <utils/kryga_log.h>
#include <utils/buffer.h>

#include <vfs/vfs.h>
#include <vfs/io.h>
#include <global_state/global_state.h>

#include <cmath>

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
vulkan_render::init(uint32_t w, uint32_t h, render_mode mode, bool only_rp)
{
    m_width = w;
    m_height = h;
    m_render_mode = mode;

    ALOG_INFO("Initializing renderer in {} mode",
              mode == render_mode::instanced ? "INSTANCED" : "PER_OBJECT");

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

        m_frames[i].buffers.objects = device.create_buffer(
            OBJECTS_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].buffers.materials = device.create_buffer(INITIAL_MATERIAL_RANGE_SIZE,
                                                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                             VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].buffers.universal_lights =
            device.create_buffer(UNIVERSAL_LIGHTS_BUFFER_SIZE,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].buffers.directional_lights =
            device.create_buffer(DIRECT_LIGHTS_BUFFER_SIZE,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].buffers.dynamic_data =
            device.create_buffer(DYNAMIC_BUFFER_SIZE * DYNAMIC_BUFFER_SIZE * 24,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

        // Cluster buffers - used by both CPU upload and GPU compute
        // CPU_TO_GPU allows CPU writes for fallback, GPU can read/write via SSBO
        m_frames[i].buffers.cluster_counts =
            device.create_buffer(DYNAMIC_BUFFER_SIZE * DYNAMIC_BUFFER_SIZE * 24,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].buffers.cluster_indices =
            device.create_buffer(DYNAMIC_BUFFER_SIZE * DYNAMIC_BUFFER_SIZE * 24,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].buffers.cluster_config = device.create_buffer(
            DYNAMIC_BUFFER_SIZE * DYNAMIC_BUFFER_SIZE * 24,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU);

        // Instance slots buffer for instanced drawing
        m_frames[i].buffers.instance_slots =
            device.create_buffer(KGPU_initial_instance_slots_size * sizeof(uint32_t),
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

        // Bone matrices SSBO for skeletal animation (initial 64KB)
        m_frames[i].buffers.bone_matrices = device.create_buffer(
            64 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        // GPU frustum culling buffers
        m_frames[i].buffers.frustum_data = device.create_buffer(sizeof(gpu::frustum_data),
                                                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].buffers.visible_indices =
            device.create_buffer(OBJECTS_BUFFER_SIZE * sizeof(uint32_t),
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_GPU_ONLY);

        m_frames[i].buffers.cull_output = device.create_buffer(
            sizeof(gpu::cull_output_data) + 64,  // Extra space for alignment
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,  // For vkCmdFillBuffer
            VMA_MEMORY_USAGE_GPU_ONLY);

        // Shadow data SSBO
        m_frames[i].buffers.shadow_data = device.create_buffer(sizeof(gpu::shadow_config_data),
                                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                               VMA_MEMORY_USAGE_CPU_TO_GPU);
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
                        KGPU_cluster_tile_size,
                        KGPU_cluster_depth_slices,
                        KGPU_max_lights_per_cluster);

    const auto& config = m_cluster_grid.get_config();
    m_cluster_config.tiles_x = config.tiles_x;
    m_cluster_config.tiles_y = config.tiles_y;
    m_cluster_config.depth_slices = config.depth_slices;
    m_cluster_config.tile_size = config.tile_size;
    m_cluster_config.near_plane = config.near_plane;
    m_cluster_config.far_plane = config.far_plane;
    m_cluster_config.log_depth_ratio = std::log(config.far_plane / config.near_plane);
    m_cluster_config.max_lights_per_cluster = config.max_lights_per_cluster;
    m_cluster_config.screen_width = config.screen_width;
    m_cluster_config.screen_height = config.screen_height;

    // Initialize per-object light grid (for non-clustered path)
    m_light_grid.init(50.0f);  // Cell size matching typical light radius

    // Initialize shadow passes
    init_shadow_passes();

    // Initialize GPU compute shaders (only needed for instanced mode)
    if (m_render_mode == render_mode::instanced)
    {
        init_cluster_cull_compute();
        init_frustum_cull_compute();
    }

    // Setup render graph based on mode
    setup_render_graph();

    // Validate all binding tables against render graph resources
    auto* main_pass = get_render_pass(AID("main"));
    if (main_pass && main_pass->are_bindings_finalized())
    {
        KRG_check(main_pass->validate_resources(m_render_graph),
                  "Main pass binding validation failed");
    }

    auto* picking_pass = get_render_pass(AID("picking"));
    if (picking_pass && picking_pass->are_bindings_finalized())
    {
        KRG_check(picking_pass->validate_resources(m_render_graph),
                  "Picking pass binding validation failed");
    }

    if (m_render_mode == render_mode::instanced && m_cluster_cull_pass &&
        m_cluster_cull_pass->are_bindings_finalized())
    {
        KRG_check(m_cluster_cull_pass->validate_resources(m_render_graph),
                  "Cluster cull pass binding validation failed");
    }

    if (m_render_mode == render_mode::instanced && m_frustum_cull_pass &&
        m_frustum_cull_pass->are_bindings_finalized())
    {
        KRG_check(m_frustum_cull_pass->validate_resources(m_render_graph),
                  "Frustum cull pass binding validation failed");
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
    m_draw_batches.clear();
    m_instance_slots_staging.clear();
    m_materials_layout = {};

    // Clear all caches before VMA is destroyed
    m_cache.objects.clear();
    m_cache.directional_lights.clear();
    m_cache.universal_lights.clear();
    m_cache.textures.clear();

    m_frames.clear();
}

void
vulkan_render::prepare_render_passes()
{
    auto& device = glob::glob_state().getr_render_device();

    {
        // Use picking preset in headless mode (TRANSFER_SRC layout, no swapchain extension needed)
        auto preset = device.is_headless() ? render_pass_builder::presets::picking
                                           : render_pass_builder::presets::swapchain;

        auto main_pass =
            render_pass_builder()
                .set_color_format(device.get_swapchain_format())
                .set_depth_format(VK_FORMAT_D32_SFLOAT_S8_UINT)
                .set_width_depth(m_width, m_height)
                .set_color_images(device.get_swapchain_image_views(), device.get_swapchain_images())
                .set_preset(preset)
                .build();

        glob::glob_state().getr_vulkan_render_loader().add_render_pass(AID("main"),
                                                                       std::move(main_pass));
    }

    VkExtent3D image_extent = {m_width, m_height, 1};

    {
        auto simg_info = vk_utils::make_image_create_info(
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            image_extent);

        VmaAllocationCreateInfo simg_allocinfo = {};
        simg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        auto image = std::make_shared<vk_utils::vulkan_image>(vk_utils::vulkan_image::create(
            glob::glob_state().getr_render_device().get_vma_allocator_provider(),
            simg_info,
            simg_allocinfo));

        auto swapchain_image_view_ci = vk_utils::make_imageview_create_info(
            VK_FORMAT_R8G8B8A8_UNORM, image->image(), VK_IMAGE_ASPECT_COLOR_BIT);

        auto image_view = vk_utils::vulkan_image_view::create_shared(swapchain_image_view_ci);

        auto ui_pass =
            render_pass_builder()
                .set_color_format(VK_FORMAT_R8G8B8A8_UNORM)
                .set_depth_format(VK_FORMAT_D32_SFLOAT)
                .set_width_depth(m_width, m_height)
                .set_color_images(std::vector<vk_utils::vulkan_image_view_sptr>{image_view},
                                  std::vector<vk_utils::vulkan_image_sptr>{image})
                .set_enable_stencil(false)
                .set_preset(render_pass_builder::presets::buffer)
                .build();

        glob::glob_state().getr_vulkan_render_loader().add_render_pass(AID("ui"),
                                                                       std::move(ui_pass));
    }

    {
        auto simg_info = vk_utils::make_image_create_info(
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            image_extent);

        VmaAllocationCreateInfo simg_allocinfo = {};
        simg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        auto image = std::make_shared<vk_utils::vulkan_image>(vk_utils::vulkan_image::create(
            glob::glob_state().getr_render_device().get_vma_allocator_provider(),
            simg_info,
            simg_allocinfo));

        auto swapchain_image_view_ci = vk_utils::make_imageview_create_info(
            VK_FORMAT_R8G8B8A8_UNORM, image->image(), VK_IMAGE_ASPECT_COLOR_BIT);

        auto image_view = vk_utils::vulkan_image_view::create_shared(swapchain_image_view_ci);

        auto picking_pass =
            render_pass_builder()
                .set_color_format(VK_FORMAT_R8G8B8A8_UNORM)
                .set_depth_format(VK_FORMAT_D32_SFLOAT)
                .set_width_depth(m_width, m_height)
                .set_color_images(std::vector<vk_utils::vulkan_image_view_sptr>{image_view},
                                  std::vector<vk_utils::vulkan_image_sptr>{image})
                .set_preset(render_pass_builder::presets::picking)
                .set_enable_stencil(false)
                .build();

        glob::glob_state().getr_vulkan_render_loader().add_render_pass(AID("picking"),
                                                                       std::move(picking_pass));
    }
}

void
vulkan_render::prepare_pass_bindings()
{
    auto* layout_cache_ptr = glob::glob_state().getr_render_device().descriptor_layout_cache();
    auto& layout_cache = *layout_cache_ptr;

    // Main pass bindings - names must match shader reflection names (dyn_ prefix)
    auto* main_pass = get_render_pass(AID("main"));
    if (main_pass)
    {
        // Set 0: Global data (camera)
        main_pass->bindings().add(AID("dyn_camera_data"),
                                  KGPU_global_descriptor_sets,
                                  0,
                                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

        // Set 1: Object data (objects, lights, clusters)
        main_pass->bindings()
            .add(AID("dyn_object_buffer"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_objects_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_directional_lights_buffer"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_directional_light_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_gpu_universal_light_data"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_universal_light_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_cluster_light_counts"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_cluster_light_counts_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_cluster_light_indices"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_cluster_light_indices_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_cluster_config"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_cluster_config_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_instance_slots"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_instance_slots_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_VERTEX_BIT)
            .add(AID("dyn_bone_matrices"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_bone_matrices_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_VERTEX_BIT)
            .add(AID("dyn_shadow_data"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_shadow_data_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

        // Set 2: Bindless textures and static samplers (managed separately from render graph)
        main_pass->bindings()
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

        // Set 3: Material data (per-material)
        main_pass->bindings().add(AID("dyn_material_buffer"),
                                  KGPU_materials_descriptor_sets,
                                  0,
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                                  VK_SHADER_STAGE_FRAGMENT_BIT,
                                  render::binding_scope::per_material);

        main_pass->finalize_bindings(layout_cache);
    }

    // Picking pass bindings - needs same bindings as main pass for shader compatibility
    auto* picking_pass = get_render_pass(AID("picking"));
    if (picking_pass)
    {
        // Set 0: Global data (camera)
        picking_pass->bindings().add(AID("dyn_camera_data"),
                                     KGPU_global_descriptor_sets,
                                     0,
                                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

        // Set 1: Object data (same as main pass)
        picking_pass->bindings()
            .add(AID("dyn_object_buffer"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_objects_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_directional_lights_buffer"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_directional_light_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_gpu_universal_light_data"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_universal_light_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_cluster_light_counts"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_cluster_light_counts_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_cluster_light_indices"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_cluster_light_indices_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_cluster_config"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_cluster_config_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_instance_slots"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_instance_slots_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_VERTEX_BIT)
            .add(AID("dyn_bone_matrices"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_bone_matrices_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_VERTEX_BIT)
            .add(AID("dyn_shadow_data"),
                 KGPU_objects_descriptor_sets,
                 KGPU_objects_shadow_data_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

        // Set 2: Bindless textures and static samplers (for common_frag.glsl compatibility)
        picking_pass->bindings()
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

        picking_pass->finalize_bindings(layout_cache);
    }

    // UI pass - simple bindings for ImGui rendering
    auto* ui_pass = get_render_pass(AID("ui"));
    if (ui_pass)
    {
        // Set 0: Font texture sampler
        ui_pass->bindings().add(AID("fontSampler"),
                                0,
                                0,
                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                VK_SHADER_STAGE_FRAGMENT_BIT,
                                render::binding_scope::per_material);

        ui_pass->finalize_bindings(layout_cache);
    }
}

void
vulkan_render::prepare_system_resources()
{
    glob::glob_state().getr_vulkan_render_loader().create_sampler(
        AID("default"), VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);

    glob::glob_state().getr_vulkan_render_loader().create_sampler(
        AID("font"), VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);

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

    vfs::load_buffer(se_base / "system/se_outline.vert", vert);
    vfs::load_buffer(se_base / "system/se_outline.frag", frag);

    se_ci.ds_mode = depth_stencil_mode::outline;

    sed = nullptr;
    rc = main_pass->create_shader_effect(AID("se_outline"), se_ci, sed);
    KRG_check(rc == result_code::ok && sed, "Always should be good!");

    std::vector<texture_sampler_data> sd;
    m_outline_mat = glob::glob_state().getr_vulkan_render_loader().create_material(
        AID("mat_outline"), AID("outline"), sd, *sed, utils::dynobj{});

    vfs::load_buffer(se_base / "system/se_pick.vert", vert);
    vfs::load_buffer(se_base / "system/se_pick.frag", frag);

    auto picking_pass =
        glob::glob_state().getr_vulkan_render_loader().get_render_pass(AID("picking"));

    se_ci.ds_mode = depth_stencil_mode::none;
    sed = nullptr;

    rc = picking_pass->create_shader_effect(AID("se_pick"), se_ci, sed);
    KRG_check(rc == result_code::ok && sed, "Always should be good!");

    m_pick_mat = glob::glob_state().getr_vulkan_render_loader().create_material(
        AID("mat_pick"), AID("pick"), sd, *sed, utils::dynobj{});

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
