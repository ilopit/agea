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

#include <tracy/Tracy.hpp>

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

    // Picking pass bindings
    auto* picking_pass = get_render_pass(AID("picking"));
    if (picking_pass)
    {
        // BDA resources used by picking
        picking_pass->bindings()
            .add_bda(AID("dyn_camera_data"))
            .add_bda(AID("dyn_object_buffer"))
            .add_bda(AID("dyn_instance_slots"))
            .add_bda(AID("dyn_bone_matrices"));

        // Set 2: Bindless textures and static samplers
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

// ============================================================================
// Shadow Pass Initialization
// ============================================================================

void
vulkan_render::init_shadow_passes()
{
    // Create 4 depth-only render passes for CSM cascades (triple-buffered)
    for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
    {
        m_shadow_passes[c] = render_pass_builder()
                                 .set_depth_format(VK_FORMAT_D32_SFLOAT)
                                 .set_depth_only(true)
                                 .set_image_count(FRAMES_IN_FLIGHT)
                                 .set_width_depth(m_render_config.shadows.map_size, m_render_config.shadows.map_size)
                                 .set_enable_stencil(false)
                                 .build();

        m_shadow_passes[c]->set_name(AID("shadow_csm_" + std::to_string(c)));
    }

    // Create local light shadow passes (spot + point DPSM), triple-buffered
    // Each local light needs up to 2 passes (point lights need front+back)
    for (uint32_t i = 0; i < KGPU_MAX_SHADOWED_LOCAL_LIGHTS * 2; ++i)
    {
        m_shadow_local_passes[i] = render_pass_builder()
                                       .set_depth_format(VK_FORMAT_D32_SFLOAT)
                                       .set_depth_only(true)
                                       .set_image_count(FRAMES_IN_FLIGHT)
                                       .set_width_depth(m_render_config.shadows.map_size, m_render_config.shadows.map_size)
                                       .set_enable_stencil(false)
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

    kryga::utils::buffer shader_buffer;
    if (!vfs::load_buffer(vfs::rid("data://shaders_includes/cluster_cull.comp"), shader_buffer))
    {
        ALOG_WARN("Failed to load cluster_cull.comp - GPU cluster culling disabled");
        return;
    }

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

    kryga::utils::buffer shader_buffer;
    if (!vfs::load_buffer(vfs::rid("data://shaders_includes/frustum_cull.comp"), shader_buffer))
    {
        ALOG_WARN("Failed to load frustum_cull.comp - GPU frustum culling disabled");
        m_gpu_frustum_culling_enabled = false;
        return;
    }

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
    switch (m_render_mode)
    {
    case render_mode::instanced:
        setup_instanced_render_graph();
        break;
    case render_mode::per_object:
        setup_per_object_render_graph();
        break;
    }
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

    m_render_graph.import_resource(AID("swapchain"), rg_resource_type::image);
    m_render_graph.import_resource(AID("ui_target"), rg_resource_type::image);
    m_render_graph.import_resource(AID("picking_target"), rg_resource_type::image);

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

    // UI pass
    m_render_graph.add_graphics_pass(AID("ui"),
                                     {m_render_graph.write(AID("ui_target"))},
                                     get_render_pass(AID("ui")),
                                     VkClearColorValue{0, 0, 0, 0},
                                     [this](VkCommandBuffer) { draw_ui(*m_current_frame); });

    // Picking pass - instanced batched drawing
    m_render_graph.add_graphics_pass(AID("picking"),
                                     {m_render_graph.write(AID("picking_target")),
                                      m_render_graph.read(AID("dyn_camera_data")),
                                      m_render_graph.read(AID("dyn_object_buffer")),
                                      m_render_graph.read(AID("dyn_cluster_light_counts")),
                                      m_render_graph.read(AID("dyn_cluster_light_indices")),
                                      m_render_graph.read(AID("dyn_cluster_config")),
                                      m_render_graph.read(AID("dyn_instance_slots")),
                                      m_render_graph.read(AID("dyn_bone_matrices"))},
                                     get_render_pass(AID("picking")),
                                     VkClearColorValue{0, 0, 0, 0},
                                     [this](VkCommandBuffer cmd) { draw_picking_instanced(cmd); });

    // Main pass - instanced batched drawing
    // Shadow maps are sampled via bindless textures. Declaring them as reads
    // ensures the render graph orders shadow passes before the main pass.
    {
        std::vector<rg_resource_ref> main_resources = {
            m_render_graph.write(AID("swapchain")),
            m_render_graph.read(AID("ui_target")),
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
        };

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

    bool result = m_render_graph.compile();
    KRG_check(result, "Instanced render graph compilation failed");
}

void
vulkan_render::setup_per_object_render_graph()
{
    // =========================================================================
    // PER-OBJECT MODE GRAPH
    // - No compute pass (CPU light grid used instead)
    // - Per-object draw calls with firstInstance = slot
    // - Identity buffer: slots[i] = i
    // =========================================================================

    // Register resources (same as instanced, but cluster buffers are CPU-filled)
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

    m_render_graph.import_resource(AID("swapchain"), rg_resource_type::image);
    m_render_graph.import_resource(AID("ui_target"), rg_resource_type::image);
    m_render_graph.import_resource(AID("picking_target"), rg_resource_type::image);

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

    // Local light shadow passes: front + back hemispheres
    for (uint32_t i = 0; i < KGPU_MAX_SHADOWED_LOCAL_LIGHTS; ++i)
    {
        auto front_name = AID("shadow_local_" + std::to_string(i));
        m_render_graph.import_resource(front_name, rg_resource_type::image);
        m_render_graph.add_graphics_pass(front_name,
                                         {m_render_graph.write(front_name),
                                          m_render_graph.read(AID("dyn_object_buffer")),
                                          m_render_graph.read(AID("dyn_instance_slots"))},
                                         m_shadow_local_passes[i * 2].get(),
                                         VkClearColorValue{},
                                         [this, i](VkCommandBuffer cmd)
                                         { draw_shadow_local_pass(cmd, i, false); });

        auto back_name = AID("shadow_local_back_" + std::to_string(i));
        m_render_graph.import_resource(back_name, rg_resource_type::image);
        m_render_graph.add_graphics_pass(back_name,
                                         {m_render_graph.write(back_name),
                                          m_render_graph.read(AID("dyn_object_buffer")),
                                          m_render_graph.read(AID("dyn_instance_slots"))},
                                         m_shadow_local_passes[i * 2 + 1].get(),
                                         VkClearColorValue{},
                                         [this, i](VkCommandBuffer cmd)
                                         { draw_shadow_local_pass(cmd, i, true); });
    }

    // NO compute pass - per-object mode uses CPU light grid

    // UI pass
    m_render_graph.add_graphics_pass(AID("ui"),
                                     {m_render_graph.write(AID("ui_target"))},
                                     get_render_pass(AID("ui")),
                                     VkClearColorValue{0, 0, 0, 0},
                                     [this](VkCommandBuffer) { draw_ui(*m_current_frame); });

    // Picking pass - per-object drawing
    m_render_graph.add_graphics_pass(AID("picking"),
                                     {m_render_graph.write(AID("picking_target")),
                                      m_render_graph.read(AID("dyn_camera_data")),
                                      m_render_graph.read(AID("dyn_object_buffer")),
                                      m_render_graph.read(AID("dyn_cluster_light_counts")),
                                      m_render_graph.read(AID("dyn_cluster_light_indices")),
                                      m_render_graph.read(AID("dyn_cluster_config")),
                                      m_render_graph.read(AID("dyn_instance_slots")),
                                      m_render_graph.read(AID("dyn_bone_matrices"))},
                                     get_render_pass(AID("picking")),
                                     VkClearColorValue{0, 0, 0, 0},
                                     [this](VkCommandBuffer cmd) { draw_picking_per_object(cmd); });

    // Main pass - per-object drawing (see instanced graph for shadow map note)
    m_render_graph.add_graphics_pass(AID("main"),
                                     {m_render_graph.write(AID("swapchain")),
                                      m_render_graph.read(AID("ui_target")),
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
                                      m_render_graph.read(AID("dyn_probe_grid"))},
                                     get_render_pass(AID("main")),
                                     VkClearColorValue{0, 0, 0, 1.0},
                                     [this](VkCommandBuffer)
                                     { draw_objects_per_object(*m_current_frame); });

    bool result = m_render_graph.compile();
    KRG_check(result, "Per-object render graph compilation failed");
}

}  // namespace render
}  // namespace kryga
