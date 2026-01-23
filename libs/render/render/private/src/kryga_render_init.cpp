#include "vulkan_render/kryga_render.h"
#include "vulkan_render/vulkan_loaders/vulkan_compute_shader_loader.h"

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"

#include <gpu_types/gpu_generic_constants.h>

#include <utils/kryga_log.h>
#include <utils/buffer.h>

#include <resource_locator/resource_locator.h>
#include <global_state/global_state.h>

#include <cmath>

namespace kryga
{
glob::vulkan_render::type glob::vulkan_render::type::s_instance;

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
vulkan_render::init(uint32_t w, uint32_t h, bool only_rp)
{
    m_width = w;
    m_height = h;

    prepare_render_passes();
    prepare_pass_bindings();

    if (only_rp)
    {
        return;
    }

    auto& device = glob::render_device::getr();

    m_frames.resize(device.frame_size());

    for (size_t i = 0; i < m_frames.size(); ++i)
    {
        m_frames[i].frame = &device.frame(i);

        m_frames[i].buffers.objects = device.create_buffer(
            OBJECTS_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].buffers.materials =
            device.create_buffer(INITIAL_MATERIAL_RANGE_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].buffers.universal_lights =
            device.create_buffer(UNIVERSAL_LIGHTS_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].buffers.directional_lights =
            device.create_buffer(DIRECT_LIGHTS_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].buffers.dynamic_data =
            device.create_buffer(DYNAMIC_BUFFER_SIZE * DYNAMIC_BUFFER_SIZE * 24,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        // Cluster buffers - used by both CPU upload and GPU compute
        // CPU_TO_GPU allows CPU writes for fallback, GPU can read/write via SSBO
        m_frames[i].buffers.cluster_counts =
            device.create_buffer(DYNAMIC_BUFFER_SIZE * DYNAMIC_BUFFER_SIZE * 24,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].buffers.cluster_indices =
            device.create_buffer(DYNAMIC_BUFFER_SIZE * DYNAMIC_BUFFER_SIZE * 24,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].buffers.cluster_config = device.create_buffer(
            DYNAMIC_BUFFER_SIZE * DYNAMIC_BUFFER_SIZE * 24,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    prepare_system_resources();
    prepare_ui_resources();
    prepare_ui_pipeline();

    // Initialize clustered lighting (must match camera near/far planes)
    m_cluster_grid.init(m_width, m_height,
                        KGPU_znear,  // near plane
                        KGPU_zfar,   // far plane - must match camera!
                        KGPU_cluster_tile_size, KGPU_cluster_depth_slices,
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

    // Initialize GPU cluster culling compute shader
    init_cluster_cull_compute();

    // Setup render graph
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

    if (m_cluster_cull_shader && m_cluster_cull_shader->are_bindings_finalized())
    {
        KRG_check(m_cluster_cull_shader->validate_resources(m_render_graph),
                  "Cluster cull shader binding validation failed");
    }
}

void
vulkan_render::deinit()
{
    m_frames.clear();
}

void
vulkan_render::prepare_render_passes()
{
    auto& device = glob::render_device::getr();

    {
        auto main_pass =
            render_pass_builder()
                .set_color_format(device.get_swapchain_format())
                .set_depth_format(VK_FORMAT_D32_SFLOAT_S8_UINT)
                .set_width_depth(m_width, m_height)
                .set_color_images(device.get_swapchain_image_views(), device.get_swapchain_images())
                .set_preset(render_pass_builder::presets::swapchain)
                .build();

        glob::vulkan_render_loader::getr().add_render_pass(AID("main"), std::move(main_pass));
    }

    VkExtent3D image_extent = {m_width, m_height, 1};

    {
        auto simg_info = vk_utils::make_image_create_info(
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, image_extent);

        VmaAllocationCreateInfo simg_allocinfo = {};
        simg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        auto image = std::make_shared<vk_utils::vulkan_image>(vk_utils::vulkan_image::create(
            glob::render_device::getr().get_vma_allocator_provider(), simg_info, simg_allocinfo));

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

        glob::vulkan_render_loader::getr().add_render_pass(AID("ui"), std::move(ui_pass));
    }

    {
        auto simg_info = vk_utils::make_image_create_info(
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, image_extent);

        VmaAllocationCreateInfo simg_allocinfo = {};
        simg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        auto image = std::make_shared<vk_utils::vulkan_image>(vk_utils::vulkan_image::create(
            glob::render_device::getr().get_vma_allocator_provider(), simg_info, simg_allocinfo));

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

        glob::vulkan_render_loader::getr().add_render_pass(AID("picking"), std::move(picking_pass));
    }
}

void
vulkan_render::prepare_pass_bindings()
{
    auto* layout_cache_ptr = glob::render_device::getr().descriptor_layout_cache();
    auto& layout_cache = *layout_cache_ptr;

    // Main pass bindings - names must match shader reflection names (dyn_ prefix)
    auto* main_pass = get_render_pass(AID("main"));
    if (main_pass)
    {
        // Set 0: Global data (camera)
        main_pass->bindings().add(
            AID("dyn_camera_data"), KGPU_global_descriptor_sets, 0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

        // Set 1: Object data (objects, lights, clusters)
        main_pass->bindings()
            .add(AID("dyn_object_buffer"), KGPU_objects_descriptor_sets,
                 KGPU_objects_objects_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_VERTEX_BIT)
            .add(AID("dyn_directional_lights_buffer"), KGPU_objects_descriptor_sets,
                 KGPU_objects_directional_light_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_gpu_universal_light_data"), KGPU_objects_descriptor_sets,
                 KGPU_objects_universal_light_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_cluster_light_counts"), KGPU_objects_descriptor_sets,
                 KGPU_objects_cluster_light_counts_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_cluster_light_indices"), KGPU_objects_descriptor_sets,
                 KGPU_objects_cluster_light_indices_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_cluster_config"), KGPU_objects_descriptor_sets,
                 KGPU_objects_cluster_config_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT);

        // Set 2: Textures (per-material, validated but bound per-draw)
        main_pass->bindings().add(AID("textures"), KGPU_textures_descriptor_sets, 0,
                                  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  VK_SHADER_STAGE_FRAGMENT_BIT, render::binding_scope::per_material);

        // Set 3: Material data (per-material)
        main_pass->bindings().add(AID("dyn_material_buffer"), KGPU_materials_descriptor_sets, 0,
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                                  VK_SHADER_STAGE_FRAGMENT_BIT, render::binding_scope::per_material);

        main_pass->finalize_bindings(layout_cache);
    }

    // Picking pass bindings - needs same bindings as main pass for shader compatibility
    auto* picking_pass = get_render_pass(AID("picking"));
    if (picking_pass)
    {
        // Set 0: Global data (camera)
        picking_pass->bindings().add(
            AID("dyn_camera_data"), KGPU_global_descriptor_sets, 0,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

        // Set 1: Object data (same as main pass)
        picking_pass->bindings()
            .add(AID("dyn_object_buffer"), KGPU_objects_descriptor_sets,
                 KGPU_objects_objects_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_VERTEX_BIT)
            .add(AID("dyn_directional_lights_buffer"), KGPU_objects_descriptor_sets,
                 KGPU_objects_directional_light_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_gpu_universal_light_data"), KGPU_objects_descriptor_sets,
                 KGPU_objects_universal_light_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_cluster_light_counts"), KGPU_objects_descriptor_sets,
                 KGPU_objects_cluster_light_counts_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_cluster_light_indices"), KGPU_objects_descriptor_sets,
                 KGPU_objects_cluster_light_indices_binding,
                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add(AID("dyn_cluster_config"), KGPU_objects_descriptor_sets,
                 KGPU_objects_cluster_config_binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                 VK_SHADER_STAGE_FRAGMENT_BIT);

        picking_pass->finalize_bindings(layout_cache);
    }

    // UI pass - simple bindings for ImGui rendering
    auto* ui_pass = get_render_pass(AID("ui"));
    if (ui_pass)
    {
        // Set 0: Font texture sampler
        ui_pass->bindings().add(AID("fontSampler"), 0, 0,
                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                VK_SHADER_STAGE_FRAGMENT_BIT, render::binding_scope::per_material);

        ui_pass->finalize_bindings(layout_cache);
    }
}

void
vulkan_render::prepare_system_resources()
{
    glob::vulkan_render_loader::getr().create_sampler(AID("default"),
                                                      VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);

    glob::vulkan_render_loader::getr().create_sampler(AID("font"),
                                                      VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);

    kryga::utils::buffer vert, frag;

    auto path = glob::glob_state().get_resource_locator()->resource(
        category::packages, "base.apkg/class/shader_effects");

    auto vert_path = path / "error/se_error.vert";
    kryga::utils::buffer::load(vert_path, vert);

    auto frag_path = path / "error/se_error.frag";
    kryga::utils::buffer::load(frag_path, frag);

    auto main_pass = glob::vulkan_render_loader::getr().get_render_pass(AID("main"));

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

    vert_path = path / "system/se_outline.vert";
    kryga::utils::buffer::load(vert_path, vert);

    frag_path = path / "system/se_outline.frag";
    kryga::utils::buffer::load(frag_path, frag);

    se_ci.ds_mode = depth_stencil_mode::outline;

    sed = nullptr;
    rc = main_pass->create_shader_effect(AID("se_outline"), se_ci, sed);
    KRG_check(rc == result_code::ok && sed, "Always should be good!");

    std::vector<texture_sampler_data> sd;
    m_outline_mat = glob::vulkan_render_loader::getr().create_material(
        AID("mat_outline"), AID("outline"), sd, *sed, utils::dynobj{});

    vert_path = path / "system/se_pick.vert";
    kryga::utils::buffer::load(vert_path, vert);

    frag_path = path / "system/se_pick.frag";
    kryga::utils::buffer::load(frag_path, frag);

    auto picking_pass = glob::vulkan_render_loader::getr().get_render_pass(AID("picking"));

    se_ci.ds_mode = depth_stencil_mode::none;
    sed = nullptr;

    rc = picking_pass->create_shader_effect(AID("se_pick"), se_ci, sed);
    KRG_check(rc == result_code::ok && sed, "Always should be good!");

    m_pick_mat = glob::vulkan_render_loader::getr().create_material(AID("mat_pick"), AID("pick"),
                                                                    sd, *sed, utils::dynobj{});
}

render_cache&
vulkan_render::get_cache()
{
    return m_cache;
}

render_pass*
vulkan_render::get_render_pass(const utils::id& id)
{
    return glob::vulkan_render_loader::getr().get_render_pass(id);
}

}  // namespace render
}  // namespace kryga
