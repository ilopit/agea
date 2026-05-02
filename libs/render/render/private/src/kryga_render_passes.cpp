#include "vulkan_render/kryga_render.h"
#include "vulkan_render/vulkan_loaders/vulkan_compute_shader_loader.h"
#include "vulkan_render/vulkan_render_loader_create_infos.h"

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/types/vulkan_render_pass_builder.h"
#include "vulkan_render/types/vulkan_compute_shader_data.h"
#include "vulkan_render/utils/vulkan_initializers.h"

#include <gpu_types/gpu_generic_constants.h>

#include <utils/kryga_log.h>
#include <utils/buffer.h>

#include <vfs/vfs.h>
#include <vfs/io.h>
#include <global_state/global_state.h>

#include <shader_system/shader_loader.h>

#include <tracy/Tracy.hpp>

#include <algorithm>

namespace kryga
{
namespace render
{

// ============================================================================
// Render Pass Creation
// ============================================================================

void
vulkan_render::prepare_render_passes()
{
    auto& device = glob::glob_state().getr_render_device();
    auto swapchain_fmt = device.get_swapchain_format();

    // Render-scale: create a reduced-resolution scene target. The main pass will
    // render into this; a composite pass will nearest-upscale it to swapchain.
    const bool render_scale = m_render_config.render_scale.enabled;
    const uint32_t scale = std::max(1u, m_render_config.render_scale.divisor);
    m_scene_lowres_width = render_scale ? std::max(1u, m_width / scale) : m_width;
    m_scene_lowres_height = render_scale ? std::max(1u, m_height / scale) : m_height;

    if (render_scale)
    {
        // Single image; the render graph serializes main-write → composite-read
        // inside one frame, so triple-buffering is not needed here.
        VkExtent3D lowres_extent = {m_scene_lowres_width, m_scene_lowres_height, 1};
        m_scene_lowres_images.clear();
        m_scene_lowres_views.clear();

        auto img_info = vk_utils::make_image_create_info(
            swapchain_fmt,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            lowres_extent);

        VmaAllocationCreateInfo img_alloc = {};
        img_alloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        auto img = std::make_shared<vk_utils::vulkan_image>(vk_utils::vulkan_image::create(
            device.get_vma_allocator_provider(), img_info, img_alloc));

        auto view_ci = vk_utils::make_imageview_create_info(
            swapchain_fmt, img->image(), VK_IMAGE_ASPECT_COLOR_BIT);
        auto view = vk_utils::vulkan_image_view::create_shared(view_ci);

        m_scene_lowres_images.push_back(std::move(img));
        m_scene_lowres_views.push_back(std::move(view));
    }

    const bool outline_enabled = render_scale && m_render_config.outline.enabled;

    // Always create main with sampled depth — VkRenderPass identity is preserved
    // across runtime render_scale toggles, so finalLayout must be ready for the
    // sampling case (composite samples main's depth for depth_outline and for
    // grid occlusion). Cost is one extra usage flag on the depth image.
    {
        auto builder = render_pass_builder()
                           .set_color_format(swapchain_fmt)
                           .set_depth_format(VK_FORMAT_D32_SFLOAT_S8_UINT)
                           .set_sampled_depth(true)
                           .set_debug_name("main");

        if (render_scale)
        {
            builder.set_width_depth(m_scene_lowres_width, m_scene_lowres_height)
                .set_color_images(m_scene_lowres_views, m_scene_lowres_images);
        }
        else
        {
            builder.set_width_depth(m_width, m_height)
                .set_color_images(device.get_swapchain_image_views(),
                                  device.get_swapchain_images());
        }

        auto main_pass = builder.build();
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
                .set_debug_name("ui")
                .build();

        glob::glob_state().getr_vulkan_render_loader().add_render_pass(AID("ui"),
                                                                       std::move(ui_pass));
    }

    // Composite pass — only needed when render_scale upscale is enabled.
    // Writes to swapchain at full resolution. Samples the scene_lowres target,
    // draws debug overlays (grid, light gizmos), then the UI overlay on top.
    // Depth format matches main_pass so pipelines created against main_pass
    // (grid, debug wireframe, billboards) remain render-pass-compatible when
    // bound inside this pass.
    if (render_scale)
    {
        auto composite_pass =
            render_pass_builder()
                .set_color_format(swapchain_fmt)
                .set_depth_format(VK_FORMAT_D32_SFLOAT_S8_UINT)
                .set_width_depth(m_width, m_height)
                .set_color_images(device.get_swapchain_image_views(), device.get_swapchain_images())
                .set_debug_name("composite")
                .build();

        glob::glob_state().getr_vulkan_render_loader().add_render_pass(AID("composite"),
                                                                       std::move(composite_pass));
    }

    // Selection mask — same format as swapchain for pipeline compatibility
    {
        auto simg_info = vk_utils::make_image_create_info(swapchain_fmt,
                                                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                              VK_IMAGE_USAGE_SAMPLED_BIT |
                                                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                          image_extent);

        VmaAllocationCreateInfo simg_allocinfo = {};
        simg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        auto image = std::make_shared<vk_utils::vulkan_image>(vk_utils::vulkan_image::create(
            glob::glob_state().getr_render_device().get_vma_allocator_provider(),
            simg_info,
            simg_allocinfo));

        auto image_view_ci = vk_utils::make_imageview_create_info(
            swapchain_fmt, image->image(), VK_IMAGE_ASPECT_COLOR_BIT);

        auto image_view = vk_utils::vulkan_image_view::create_shared(image_view_ci);

        auto mask_pass =
            render_pass_builder()
                .set_color_format(swapchain_fmt)
                .set_depth_format(VK_FORMAT_D32_SFLOAT_S8_UINT)
                .set_width_depth(m_width, m_height)
                .set_color_images(std::vector<vk_utils::vulkan_image_view_sptr>{image_view},
                                  std::vector<vk_utils::vulkan_image_sptr>{image})
                .set_debug_name("selection_mask")
                .build();

        glob::glob_state().getr_vulkan_render_loader().add_render_pass(AID("selection_mask"),
                                                                       std::move(mask_pass));
    }
}

// ============================================================================
// Pass Descriptor Bindings
// ============================================================================

void
vulkan_render::prepare_pass_bindings()
{
    auto* layout_cache_ptr = glob::glob_state().getr_render_device().descriptor_layout_cache();
    auto& layout_cache = *layout_cache_ptr;

    // Main pass bindings
    auto* main_pass = get_render_pass(AID("main"));
    if (main_pass)
    {
        // BDA resources — accessed via pointer table, tracked for per-frame validation
        main_pass->bindings()
            .add_bda(AID("dyn_camera_data"))
            .add_bda(AID("dyn_object_buffer"))
            .add_bda(AID("dyn_directional_lights_buffer"))
            .add_bda(AID("dyn_gpu_universal_light_data"))
            .add_bda(AID("dyn_cluster_light_counts"))
            .add_bda(AID("dyn_cluster_light_indices"))
            .add_bda(AID("dyn_cluster_config"))
            .add_bda(AID("dyn_instance_slots"))
            .add_bda(AID("dyn_bone_matrices"))
            .add_bda(AID("dyn_shadow_data"))
            .add_bda(AID("dyn_probe_data"))
            .add_bda(AID("dyn_probe_grid"))
            .add_bda(AID("dyn_material_buffer"));

        // Set 2: Bindless textures and static samplers (only remaining descriptor set)
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

        // Set 3: Material data — now accessed via BDA pointer table

        main_pass->finalize_bindings(layout_cache);
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

    // Selection mask pass — same BDA as main pass (needs camera + objects + instances)
    auto* mask_pass = get_render_pass(AID("selection_mask"));
    if (mask_pass)
    {
        mask_pass->bindings()
            .add_bda(AID("dyn_camera_data"))
            .add_bda(AID("dyn_object_buffer"))
            .add_bda(AID("dyn_instance_slots"))
            .add_bda(AID("dyn_bone_matrices"));

        mask_pass->bindings()
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

        mask_pass->finalize_bindings(layout_cache);
    }

    // Composite pass (render_scale only) — needs bindless textures to sample the
    // low-res scene target by its bindless index.
    if (auto* composite_pass = get_render_pass(AID("composite")); composite_pass)
    {
        composite_pass->bindings()
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

        composite_pass->finalize_bindings(layout_cache);
    }
}

// ============================================================================
// Shadow Pass Initialization
// ============================================================================

void
vulkan_render::init_shadow_passes()
{
    // Create 4 depth-only render passes for CSM cascades (triple-buffered)
    for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
    {
        m_shadow_passes[c] =
            render_pass_builder()
                .set_depth_format(VK_FORMAT_D32_SFLOAT)
                .set_depth_only(true)
                .set_image_count(FRAMES_IN_FLIGHT)
                .set_width_depth(m_render_config.shadows.map_size, m_render_config.shadows.map_size)
                .set_enable_stencil(false)
                .set_debug_name("shadow_csm_" + std::to_string(c))
                .build();

        m_shadow_passes[c]->set_name(AID("shadow_csm_" + std::to_string(c)));
    }

    // Create local light shadow passes (spot + point DPSM), triple-buffered
    // Each local light needs up to 2 passes (point lights need front+back)
    for (uint32_t i = 0; i < KGPU_MAX_SHADOWED_LOCAL_LIGHTS * 2; ++i)
    {
        m_shadow_local_passes[i] =
            render_pass_builder()
                .set_depth_format(VK_FORMAT_D32_SFLOAT)
                .set_depth_only(true)
                .set_image_count(FRAMES_IN_FLIGHT)
                .set_width_depth(m_render_config.shadows.map_size, m_render_config.shadows.map_size)
                .set_enable_stencil(false)
                .set_debug_name("shadow_local_" + std::to_string(i))
                .build();

        m_shadow_local_passes[i]->set_name(AID("shadow_local_" + std::to_string(i)));
    }
}

// ============================================================================
// Compute Pass Initialization
// ============================================================================

void
vulkan_render::init_cluster_cull_compute()
{
    ZoneScopedN("Render::InitClusterCullCompute");

    auto shader_buffer_r = render::shader_loader::load(vfs::rid("data://shaders_includes/cluster_cull.comp.spv"));
    if (!shader_buffer_r)
    {
        ALOG_WARN("Failed to load cluster_cull.comp.spv - GPU cluster culling disabled");
        return;
    }
    auto& shader_buffer = *shader_buffer_r;

    // Create compute pass - bindings are owned by the pass, shader is created through it
    m_cluster_cull_pass = std::make_shared<render_pass>(AID("cluster_cull"), rg_pass_type::compute);

    // Declare bindings for cluster cull compute shader on the pass
    // Names must match render graph resource names (dyn_ prefix)
    // set=0, binding=0: ClusterConfig (uniform)
    // set=0, binding=1: CameraData (uniform)
    // set=0, binding=2: LightBuffer (storage, readonly)
    // set=0, binding=3: ClusterLightCounts (storage, writeonly)
    // set=0, binding=4: ClusterLightIndices (storage, writeonly)
    m_cluster_cull_pass->bindings()
        .add(AID("dyn_cluster_config"),
             0,
             0,
             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT)
        .add(AID("dyn_camera_data"),
             0,
             1,
             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT)
        .add(AID("dyn_gpu_universal_light_data"),
             0,
             2,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT)
        .add(AID("dyn_cluster_light_counts"),
             0,
             3,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT)
        .add(AID("dyn_cluster_light_indices"),
             0,
             4,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT);

    m_cluster_cull_pass->finalize_bindings(
        *glob::glob_state().getr_render_device().descriptor_layout_cache());

    // Create compute shader through the pass
    compute_shader_create_info info;
    info.shader_buffer = &shader_buffer;

    auto rc = m_cluster_cull_pass->create_compute_shader(
        AID("cluster_cull"), info, m_cluster_cull_shader);
    if (rc != result_code::ok)
    {
        ALOG_WARN("Failed to create cluster cull compute shader - GPU cluster culling disabled");
        m_cluster_cull_pass.reset();
        m_cluster_cull_shader = nullptr;
        return;
    }

    ALOG_INFO("GPU cluster culling compute shader initialized");
}

void
vulkan_render::init_frustum_cull_compute()
{
    ZoneScopedN("Render::InitFrustumCullCompute");

    auto shader_buffer_r = render::shader_loader::load(vfs::rid("data://shaders_includes/frustum_cull.comp.spv"));
    if (!shader_buffer_r)
    {
        ALOG_WARN("Failed to load frustum_cull.comp - GPU frustum culling disabled");
        m_gpu_frustum_culling_enabled = false;
        return;
    }
    auto& shader_buffer = *shader_buffer_r;

    // Create compute pass - bindings are owned by the pass
    m_frustum_cull_pass = std::make_shared<render_pass>(AID("frustum_cull"), rg_pass_type::compute);

    // Declare bindings for frustum cull compute shader
    // set=0, binding=0: FrustumBuffer (uniform)
    // set=0, binding=1: ObjectBuffer (storage, readonly)
    // set=0, binding=2: VisibleIndices (storage, writeonly)
    // set=0, binding=3: CullOutput (storage)
    m_frustum_cull_pass->bindings()
        .add(AID("dyn_frustum_data"),
             0,
             0,
             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT)
        .add(AID("dyn_object_buffer"),
             0,
             1,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT)
        .add(AID("dyn_visible_indices"),
             0,
             2,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT)
        .add(AID("dyn_cull_output"),
             0,
             3,
             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
             VK_SHADER_STAGE_COMPUTE_BIT);

    m_frustum_cull_pass->finalize_bindings(
        *glob::glob_state().getr_render_device().descriptor_layout_cache());

    // Create compute shader through the pass
    compute_shader_create_info info;
    info.shader_buffer = &shader_buffer;

    auto rc = m_frustum_cull_pass->create_compute_shader(
        AID("frustum_cull"), info, m_frustum_cull_shader);
    if (rc != result_code::ok)
    {
        ALOG_WARN("Failed to create frustum cull compute shader - GPU frustum culling disabled");
        m_frustum_cull_pass.reset();
        m_frustum_cull_shader = nullptr;
        m_gpu_frustum_culling_enabled = false;
        return;
    }

    m_gpu_frustum_culling_enabled = true;
    ALOG_INFO("GPU frustum culling compute shader initialized");
}

// ============================================================================
// Render Graph Setup
// ============================================================================

void
vulkan_render::setup_render_graph()
{
    setup_instanced_render_graph();
}

void
vulkan_render::setup_instanced_render_graph()
{
    // =========================================================================
    // INSTANCED MODE GRAPH
    // - GPU cluster culling compute pass
    // - Batched instanced drawing
    // - instance_slots buffer maps gl_InstanceIndex -> object slot
    // =========================================================================

    // Register resources
    m_render_graph.register_buffer(AID("dyn_camera_data"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_object_buffer"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_gpu_universal_light_data"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_directional_lights_buffer"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_cluster_light_counts"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_cluster_light_indices"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_cluster_config"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_instance_slots"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_bone_matrices"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_material_buffer"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_shadow_data"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_probe_data"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_probe_grid"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // GPU frustum culling buffers
    m_render_graph.register_buffer(AID("dyn_frustum_data"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_visible_indices"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_cull_output"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    const bool render_scale = m_render_config.render_scale.enabled;

    m_render_graph.import_resource(AID("swapchain"), rg_resource_type::image);
    m_render_graph.import_resource(AID("ui_target"), rg_resource_type::image);
    m_render_graph.import_resource(AID("selection_mask_target"), rg_resource_type::image);
    if (render_scale)
    {
        m_render_graph.import_resource(AID("scene_lowres_target"), rg_resource_type::image);
    }

    // Shadow passes (CSM cascades)
    for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
    {
        auto pass_name = AID("shadow_csm_" + std::to_string(c));
        m_render_graph.import_resource(pass_name, rg_resource_type::image);
        m_render_graph.add_graphics_pass(pass_name,
                                         {m_render_graph.write(pass_name),
                                          m_render_graph.read(AID("dyn_object_buffer")),
                                          m_render_graph.read(AID("dyn_instance_slots"))},
                                         m_shadow_passes[c].get(),
                                         VkClearColorValue{},
                                         [this, c](VkCommandBuffer cmd)
                                         { draw_shadow_pass(cmd, c); });
    }

    // Local light shadow passes: front hemisphere (spot + point front)
    for (uint32_t i = 0; i < KGPU_MAX_SHADOWED_LOCAL_LIGHTS; ++i)
    {
        auto pass_name = AID("shadow_local_" + std::to_string(i));
        m_render_graph.import_resource(pass_name, rg_resource_type::image);
        m_render_graph.add_graphics_pass(pass_name,
                                         {m_render_graph.write(pass_name),
                                          m_render_graph.read(AID("dyn_object_buffer")),
                                          m_render_graph.read(AID("dyn_instance_slots"))},
                                         m_shadow_local_passes[i * 2].get(),
                                         VkClearColorValue{},
                                         [this, i](VkCommandBuffer cmd)
                                         { draw_shadow_local_pass(cmd, i, false); });
    }

    // Local light shadow passes: back hemisphere (point lights DPSM only)
    for (uint32_t i = 0; i < KGPU_MAX_SHADOWED_LOCAL_LIGHTS; ++i)
    {
        auto pass_name = AID("shadow_local_back_" + std::to_string(i));
        m_render_graph.import_resource(pass_name, rg_resource_type::image);
        m_render_graph.add_graphics_pass(pass_name,
                                         {m_render_graph.write(pass_name),
                                          m_render_graph.read(AID("dyn_object_buffer")),
                                          m_render_graph.read(AID("dyn_instance_slots"))},
                                         m_shadow_local_passes[i * 2 + 1].get(),
                                         VkClearColorValue{},
                                         [this, i](VkCommandBuffer cmd)
                                         { draw_shadow_local_pass(cmd, i, true); });
    }

    // Compute pass: GPU frustum culling (runs before cluster culling)
    // Frustum culling is required for instanced mode - dispatch_frustum_cull_impl asserts if not
    // ready
    m_render_graph.add_compute_pass(AID("frustum_cull"),
                                    {m_render_graph.read(AID("dyn_frustum_data")),
                                     m_render_graph.read(AID("dyn_object_buffer")),
                                     m_render_graph.write(AID("dyn_visible_indices")),
                                     m_render_graph.write(AID("dyn_cull_output"))},
                                    [this](VkCommandBuffer cmd)
                                    { dispatch_frustum_cull_impl(cmd); });

    // Compute pass: GPU cluster culling
    m_render_graph.add_compute_pass(AID("cluster_cull"),
                                    {m_render_graph.write(AID("dyn_cluster_light_counts")),
                                     m_render_graph.write(AID("dyn_cluster_light_indices")),
                                     m_render_graph.read(AID("dyn_gpu_universal_light_data"))},
                                    [this](VkCommandBuffer cmd)
                                    {
                                        if (m_cluster_cull_shader)
                                        {
                                            dispatch_cluster_cull_impl(cmd);
                                        }
                                    });

    // Selection mask pass — render outlined objects as flat white to R8 mask
    m_render_graph.add_graphics_pass(AID("selection_mask"),
                                     {m_render_graph.write(AID("selection_mask_target")),
                                      m_render_graph.read(AID("dyn_camera_data")),
                                      m_render_graph.read(AID("dyn_object_buffer")),
                                      m_render_graph.read(AID("dyn_instance_slots")),
                                      m_render_graph.read(AID("dyn_bone_matrices"))},
                                     get_render_pass(AID("selection_mask")),
                                     VkClearColorValue{0, 0, 0, 0},
                                     [this](VkCommandBuffer cmd)
                                     { draw_selection_mask(cmd, *m_current_frame); });

    // UI pass
    m_render_graph.add_graphics_pass(AID("ui"),
                                     {m_render_graph.write(AID("ui_target"))},
                                     get_render_pass(AID("ui")),
                                     VkClearColorValue{0, 0, 0, 0},
                                     [this](VkCommandBuffer) { draw_ui(*m_current_frame); });

    // Main pass - instanced batched drawing
    // Shadow maps are sampled via bindless textures. Declaring them as reads
    // ensures the render graph orders shadow passes before the main pass.
    {
        auto main_write = render_scale ? m_render_graph.write(AID("scene_lowres_target"))
                                       : m_render_graph.write(AID("swapchain"));

        std::vector<rg_resource_ref> main_resources = {
            main_write,
            m_render_graph.read(AID("dyn_camera_data")),
            m_render_graph.read(AID("dyn_object_buffer")),
            m_render_graph.read(AID("dyn_gpu_universal_light_data")),
            m_render_graph.read(AID("dyn_directional_lights_buffer")),
            m_render_graph.read(AID("dyn_cluster_light_counts")),
            m_render_graph.read(AID("dyn_cluster_light_indices")),
            m_render_graph.read(AID("dyn_cluster_config")),
            m_render_graph.read(AID("dyn_instance_slots")),
            m_render_graph.read(AID("dyn_bone_matrices")),
            m_render_graph.read(AID("dyn_material_buffer")),
            m_render_graph.read(AID("dyn_shadow_data")),
            m_render_graph.read(AID("dyn_probe_data")),
            m_render_graph.read(AID("dyn_probe_grid")),
            m_render_graph.read(AID("selection_mask_target")),
        };

        // When render_scale is off, main pass also composites UI on top of swapchain.
        // When on, UI is drawn in the composite pass instead.
        if (!render_scale)
        {
            main_resources.push_back(m_render_graph.read(AID("ui_target")));
        }

        // Shadow map dependencies (ordering only — actual sampling is via bindless)
        for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
        {
            main_resources.push_back(m_render_graph.read(AID("shadow_csm_" + std::to_string(c))));
        }
        for (uint32_t i = 0; i < KGPU_MAX_SHADOWED_LOCAL_LIGHTS; ++i)
        {
            main_resources.push_back(m_render_graph.read(AID("shadow_local_" + std::to_string(i))));
            main_resources.push_back(
                m_render_graph.read(AID("shadow_local_back_" + std::to_string(i))));
        }

        m_render_graph.add_graphics_pass(AID("main"),
                                         std::move(main_resources),
                                         get_render_pass(AID("main")),
                                         VkClearColorValue{0, 0, 0, 1.0},
                                         [this](VkCommandBuffer)
                                         { draw_objects_instanced(*m_current_frame); });
    }

    // Composite pass — upscales the low-res scene target to swapchain, then draws UI.
    // Only present when render_scale is enabled.
    if (render_scale)
    {
        m_render_graph.add_graphics_pass(AID("composite"),
                                         {m_render_graph.write(AID("swapchain")),
                                          m_render_graph.read(AID("scene_lowres_target")),
                                          m_render_graph.read(AID("ui_target"))},
                                         get_render_pass(AID("composite")),
                                         VkClearColorValue{0, 0, 0, 1.0},
                                         [this](VkCommandBuffer cmd)
                                         { draw_composite(cmd, *m_current_frame); });
    }

    // Swapchain needs a final layout transition after the last pass

    m_render_graph.set_final_layout(AID("swapchain"),
                                    glob::glob_state().getr_render_device().is_headless()
                                        ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                                        : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    bool result = m_render_graph.compile();
    KRG_check(result, "Instanced render graph compilation failed");
}

}  // namespace render
}  // namespace kryga
