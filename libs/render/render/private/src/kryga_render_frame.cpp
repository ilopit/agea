#include "vulkan_render/kryga_render.h"

#include <tracy/Tracy.hpp>

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/types/vulkan_sampler_data.h"

#include <gpu_types/gpu_generic_constants.h>
#include <gpu_types/gpu_shadow_types.h>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <native/native_window.h>

#include <global_state/global_state.h>

#include <utils/kryga_log.h>

#include <imgui.h>

#include <cmath>

namespace kryga
{
namespace render
{

void
vulkan_render::set_camera(gpu::camera_data c)
{
    // Mark clusters dirty if view changed (light-cluster assignment is in view space)
    if (m_camera_data.view != c.view)
    {
        m_clusters_dirty = true;
    }
    m_camera_data = c;
    m_frustum.extract_planes(c.projection * c.view);
}

void
vulkan_render::draw_main()
{
    ZoneScopedN("Render::DrawMain");

    auto device = glob::glob_state().get_render_device();
    KRG_check(!device->is_headless(), "draw_main requires a windowed device, use draw_headless()");

    auto r = SDL_GetWindowFlags(glob::glob_state().getr_native_window().handle());

    if ((SDL_WINDOW_MINIMIZED & r) == SDL_WINDOW_MINIMIZED)
    {
        return;
    }

    device->switch_frame_indeces();
    m_culled_draws = 0;
    m_all_draws = 0;

    auto& current_frame = m_frames[device->get_current_frame_index()];

    {
        ZoneScopedN("Render::WaitForFence");
        VK_CHECK(vkWaitForFences(
            device->vk_device(), 1, &current_frame.frame->m_render_fence, true, 1000000000));
    }
    VK_CHECK(vkResetFences(device->vk_device(), 1, &current_frame.frame->m_render_fence));

    current_frame.frame->m_dynamic_descriptor_allocator->reset_pools();
    VK_CHECK(vkResetCommandBuffer(current_frame.frame->m_main_command_buffer, 0));

    // Acquire swapchain image
    uint32_t swapchain_image_index = 0U;
    VK_CHECK(vkAcquireNextImageKHR(device->vk_device(),
                                   device->swapchain(),
                                   1000000000,
                                   current_frame.frame->m_present_semaphore,
                                   nullptr,
                                   &swapchain_image_index));

    auto cmd = current_frame.frame->m_main_command_buffer;
    auto cmd_begin_info =
        vk_utils::make_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

    auto width = (uint32_t)glob::glob_state().get_native_window()->get_size().w;
    auto height = (uint32_t)glob::glob_state().get_native_window()->get_size().h;

    update_ui(current_frame);

    render_frame(cmd, current_frame, swapchain_image_index, width, height);

    VK_CHECK(vkEndCommandBuffer(cmd));

    // Submit with present semaphores
    auto submit = render::vk_utils::make_submit_info(&cmd);
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    submit.pWaitDstStageMask = &wait_stage;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &current_frame.frame->m_present_semaphore;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &current_frame.frame->m_render_semaphore;

    VK_CHECK(vkQueueSubmit(
        device->vk_graphics_queue(), 1, &submit, current_frame.frame->m_render_fence));

    // Present
    auto present_info = render::vk_utils::make_present_info();
    present_info.pSwapchains = &device->swapchain();
    present_info.swapchainCount = 1;
    present_info.pWaitSemaphores = &current_frame.frame->m_render_semaphore;
    present_info.waitSemaphoreCount = 1;
    present_info.pImageIndices = &swapchain_image_index;

    VK_CHECK(vkQueuePresentKHR(device->vk_graphics_queue(), &present_info));
}

void
vulkan_render::draw_headless()
{
    auto device = glob::glob_state().get_render_device();
    KRG_check(device->is_headless(), "draw_headless requires a headless device, use draw_main()");

    device->switch_frame_indeces();
    m_culled_draws = 0;
    m_all_draws = 0;

    auto& current_frame = m_frames[device->get_current_frame_index()];

    VK_CHECK(vkWaitForFences(
        device->vk_device(), 1, &current_frame.frame->m_render_fence, true, 1000000000));
    VK_CHECK(vkResetFences(device->vk_device(), 1, &current_frame.frame->m_render_fence));

    current_frame.frame->m_dynamic_descriptor_allocator->reset_pools();
    VK_CHECK(vkResetCommandBuffer(current_frame.frame->m_main_command_buffer, 0));

    auto cmd = current_frame.frame->m_main_command_buffer;
    auto cmd_begin_info =
        vk_utils::make_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

    render_frame(cmd, current_frame, 0, m_width, m_height);

    VK_CHECK(vkEndCommandBuffer(cmd));

    // Submit without present semaphores and wait synchronously
    auto submit = render::vk_utils::make_submit_info(&cmd);
    VK_CHECK(vkQueueSubmit(
        device->vk_graphics_queue(), 1, &submit, current_frame.frame->m_render_fence));

    VK_CHECK(vkWaitForFences(
        device->vk_device(), 1, &current_frame.frame->m_render_fence, true, 1000000000));
}

void
vulkan_render::render_frame(VkCommandBuffer cmd,
                            frame_state& current_frame,
                            uint32_t swapchain_image_index,
                            uint32_t width,
                            uint32_t height)
{
    prepare_draw_resources(current_frame);

    m_current_frame = &current_frame;
    m_bda_material_bound = false;
    m_render_graph.set_frame_context(swapchain_image_index, width, height);

    // Build descriptor set for cluster culling (required for instanced mode)
    if (is_instanced_mode())
    {
        KRG_check(m_cluster_cull_pass, "Cluster cull pass required for instanced mode");
        KRG_check(m_cluster_cull_pass->are_bindings_finalized(),
                  "Cluster cull bindings not finalized");

        m_cluster_cull_pass->begin_frame();
        m_cluster_cull_pass->bind(AID("dyn_cluster_config"), current_frame.buffers.cluster_config);
        m_cluster_cull_pass->bind(AID("dyn_camera_data"), current_frame.buffers.dynamic_data);
        m_cluster_cull_pass->bind(AID("dyn_gpu_universal_light_data"),
                                  current_frame.buffers.universal_lights);
        m_cluster_cull_pass->bind(AID("dyn_cluster_light_counts"),
                                  current_frame.buffers.cluster_counts);
        m_cluster_cull_pass->bind(AID("dyn_cluster_light_indices"),
                                  current_frame.buffers.cluster_indices);

        m_cluster_cull_descriptor_set = m_cluster_cull_pass->get_descriptor_set(
            0, *current_frame.frame->m_dynamic_descriptor_allocator);
    }

    // Build descriptor set for frustum culling (required for instanced mode)
    if (is_instanced_mode())
    {
        KRG_check(m_frustum_cull_pass, "Frustum cull pass required for instanced mode");
        KRG_check(m_gpu_frustum_culling_enabled, "GPU frustum culling required for instanced mode");
        KRG_check(m_frustum_cull_pass->are_bindings_finalized(),
                  "Frustum cull bindings not finalized");

        m_frustum_cull_pass->begin_frame();
        m_frustum_cull_pass->bind(AID("dyn_frustum_data"), current_frame.buffers.frustum_data);
        m_frustum_cull_pass->bind(AID("dyn_object_buffer"), current_frame.buffers.objects);
        m_frustum_cull_pass->bind(AID("dyn_visible_indices"),
                                  current_frame.buffers.visible_indices);
        m_frustum_cull_pass->bind(AID("dyn_cull_output"), current_frame.buffers.cull_output);

        m_frustum_cull_descriptor_set = m_frustum_cull_pass->get_descriptor_set(
            0, *current_frame.frame->m_dynamic_descriptor_allocator);
    }

    m_render_graph.begin_frame();

    // Bind per-frame buffer resources
    m_render_graph.bind_buffer(AID("dyn_camera_data"), current_frame.buffers.dynamic_data);
    m_render_graph.bind_buffer(AID("dyn_object_buffer"), current_frame.buffers.objects);
    m_render_graph.bind_buffer(AID("dyn_gpu_universal_light_data"),
                               current_frame.buffers.universal_lights);
    m_render_graph.bind_buffer(AID("dyn_directional_lights_buffer"),
                               current_frame.buffers.directional_lights);
    m_render_graph.bind_buffer(AID("dyn_cluster_light_counts"),
                               current_frame.buffers.cluster_counts);
    m_render_graph.bind_buffer(AID("dyn_cluster_light_indices"),
                               current_frame.buffers.cluster_indices);
    m_render_graph.bind_buffer(AID("dyn_cluster_config"), current_frame.buffers.cluster_config);
    m_render_graph.bind_buffer(AID("dyn_instance_slots"), current_frame.buffers.instance_slots);
    m_render_graph.bind_buffer(AID("dyn_bone_matrices"), current_frame.buffers.bone_matrices);
    m_render_graph.bind_buffer(AID("dyn_material_buffer"), current_frame.buffers.materials);
    m_render_graph.bind_buffer(AID("dyn_shadow_data"), current_frame.buffers.shadow_data);
    m_render_graph.bind_buffer(AID("dyn_probe_data"), current_frame.buffers.probe_data);
    m_render_graph.bind_buffer(AID("dyn_probe_grid"), current_frame.buffers.probe_grid);

    if (is_instanced_mode())
    {
        m_render_graph.bind_buffer(AID("dyn_frustum_data"), current_frame.buffers.frustum_data);
        m_render_graph.bind_buffer(AID("dyn_visible_indices"),
                                   current_frame.buffers.visible_indices);
        m_render_graph.bind_buffer(AID("dyn_cull_output"), current_frame.buffers.cull_output);
    }

    // Bind per-frame image resources
    auto* main_pass = get_render_pass(AID("main"));
    auto* ui_pass = get_render_pass(AID("ui"));
    auto* picking_pass = get_render_pass(AID("picking"));

    auto main_images = main_pass->get_color_images();
    m_render_graph.bind_image(AID("swapchain"), *main_images[swapchain_image_index]);

    auto ui_images = ui_pass->get_color_images();
    m_render_graph.bind_image(AID("ui_target"), *ui_images[0]);

    auto picking_images = picking_pass->get_color_images();
    m_render_graph.bind_image(AID("picking_target"), *picking_images[0]);

    for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
    {
        if (m_shadow_passes[c])
        {
            auto& depth_images = m_shadow_passes[c]->get_depth_images();
            if (!depth_images.empty())
            {
                m_render_graph.bind_image(AID("shadow_csm_" + std::to_string(c)), depth_images[0]);
            }
        }
    }

    for (uint32_t i = 0; i < KGPU_MAX_SHADOWED_LOCAL_LIGHTS; ++i)
    {
        if (m_shadow_local_passes[i * 2])
        {
            auto& front = m_shadow_local_passes[i * 2]->get_depth_images();
            if (!front.empty())
            {
                m_render_graph.bind_image(AID("shadow_local_" + std::to_string(i)), front[0]);
            }
        }
        if (m_shadow_local_passes[i * 2 + 1])
        {
            auto& back = m_shadow_local_passes[i * 2 + 1]->get_depth_images();
            if (!back.empty())
            {
                m_render_graph.bind_image(AID("shadow_local_back_" + std::to_string(i)), back[0]);
            }
        }
    }

    m_render_graph.execute(
        cmd, glob::glob_state().getr_render_device().get_current_frame_index(), width, height);

    // Verify per-draw BDA addresses were set this frame (if any objects were drawn)
    if (m_all_draws > 0)
    {
        KRG_check(m_bda_material_bound,
                  "BDA material address was never set this frame — "
                  "bind_material() must be called before drawing objects");
    }
}

void
vulkan_render::prepare_draw_resources(render::frame_state& current_frame)
{
    ZoneScopedN("Render::PrepareResources");

    // Apply any runtime config changes (cluster reinit, render mode switch)
    apply_config_changes();

    // Update bindless texture descriptors for any newly registered textures
    update_bindless_descriptors();

    // Build instance data (batches for instanced path, identity buffer for legacy path)
    // Both paths need the instance_slots buffer populated for shaders to work
    prepare_instance_data(current_frame);

    // Upload bone matrices for skeletal animation
    upload_bone_matrices(current_frame);

    if (current_frame.uploads.has_objects())
    {
        ZoneScopedN("Render::UploadObjects");
        upload_obj_data(current_frame);
        current_frame.uploads.objects_queue.clear();
    }

    if (current_frame.uploads.has_universal_lights())
    {
        ZoneScopedN("Render::UploadLights");
        upload_universal_light_data(current_frame);
        current_frame.uploads.universal_light_queue.clear();
    }

    if (current_frame.uploads.has_directional_lights())
    {
        upload_directional_light_data(current_frame);
        current_frame.uploads.directional_light_queue.clear();
    }

    if (current_frame.uploads.has_materials())
    {
        ZoneScopedN("Render::UploadMaterials");
        upload_material_data(current_frame);
        current_frame.uploads.has_pending_materials = false;
    }

    auto& dyn = current_frame.buffers.dynamic_data;

    dyn.begin();
    dyn.upload_data(m_camera_data);
    dyn.end();

    if (is_instanced_mode())
    {
        // Upload frustum data for GPU frustum culling (required for instanced mode)
        KRG_check(m_frustum_cull_pass, "Frustum cull pass required for instanced mode");
        KRG_check(m_gpu_frustum_culling_enabled, "GPU frustum culling required for instanced mode");
        upload_frustum_data(current_frame);

        // GPU cluster culling required for instanced mode
        KRG_check(m_cluster_cull_pass, "Cluster cull pass required for instanced mode");
        KRG_check(m_cluster_cull_shader, "Cluster cull shader required for instanced mode");

        // GPU compute path: upload config and dispatch compute shader
        ZoneScopedN("Render::GPUClusterCull");

        // Upload cluster config (needed by compute shader)
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

        current_frame.buffers.cluster_config.begin();
        auto* data =
            current_frame.buffers.cluster_config.allocate_data(sizeof(gpu::cluster_grid_data));
        memcpy(data, &m_cluster_config, sizeof(gpu::cluster_grid_data));
        current_frame.buffers.cluster_config.end();

        // Dispatch compute shader will be called after command buffer begins
        m_clusters_dirty = false;
    }
    else
    {
        // Per-object mode: rebuild light grid when lights changed
        if (m_light_grid_dirty)
        {
            ZoneScopedN("Render::RebuildLightGrid");
            rebuild_light_grid();
            m_light_grid_dirty = false;
        }
    }

    // Bind BDA resources — validates each buffer is available this frame
    auto* main_pass = get_render_pass(AID("main"));
    if (main_pass && main_pass->are_bindings_finalized())
    {
        main_pass->begin_frame();
        main_pass->bind(AID("dyn_camera_data"), current_frame.buffers.dynamic_data);
        main_pass->bind(AID("dyn_object_buffer"), current_frame.buffers.objects);
        main_pass->bind(AID("dyn_directional_lights_buffer"),
                        current_frame.buffers.directional_lights);
        main_pass->bind(AID("dyn_gpu_universal_light_data"),
                        current_frame.buffers.universal_lights);
        main_pass->bind(AID("dyn_cluster_light_counts"), current_frame.buffers.cluster_counts);
        main_pass->bind(AID("dyn_cluster_light_indices"), current_frame.buffers.cluster_indices);
        main_pass->bind(AID("dyn_cluster_config"), current_frame.buffers.cluster_config);
        main_pass->bind(AID("dyn_instance_slots"), current_frame.buffers.instance_slots);
        main_pass->bind(AID("dyn_bone_matrices"), current_frame.buffers.bone_matrices);
        main_pass->bind(AID("dyn_shadow_data"), current_frame.buffers.shadow_data);
        main_pass->bind(AID("dyn_probe_data"), current_frame.buffers.probe_data);
        main_pass->bind(AID("dyn_probe_grid"), current_frame.buffers.probe_grid);
        main_pass->bind(AID("dyn_material_buffer"), current_frame.buffers.materials);

        KRG_check(static_cast<const render_pass*>(main_pass)->bindings().validate_bda_bound(),
                  "Main pass: not all BDA resources bound this frame");
    }

    auto* picking_pass = get_render_pass(AID("picking"));
    if (picking_pass && picking_pass->are_bindings_finalized())
    {
        picking_pass->begin_frame();
        picking_pass->bind(AID("dyn_camera_data"), current_frame.buffers.dynamic_data);
        picking_pass->bind(AID("dyn_object_buffer"), current_frame.buffers.objects);
        picking_pass->bind(AID("dyn_instance_slots"), current_frame.buffers.instance_slots);
        picking_pass->bind(AID("dyn_bone_matrices"), current_frame.buffers.bone_matrices);

        KRG_check(static_cast<const render_pass*>(picking_pass)->bindings().validate_bda_bound(),
                  "Picking pass: not all BDA resources bound this frame");
    }

    // Prepare debug light visualization data (must happen before rendering)
    prepare_debug_light_data(current_frame);

    // Sync render_config → GPU shadow config (UI may have changed values)
    m_shadow_config.directional.shadow_bias = m_render_config.shadows.bias;
    m_shadow_config.directional.normal_bias = m_render_config.shadows.normal_bias;
    m_shadow_config.directional.pcf_mode = static_cast<uint32_t>(m_render_config.shadows.pcf);
    m_shadow_config.directional.cascade_count = m_render_config.shadows.cascade_count;
    m_shadow_config.directional.texel_size =
        1.0f / static_cast<float>(m_render_config.shadows.map_size);

    // Upload shadow data
    upload_shadow_data(current_frame);

    // Fill BDA addresses directly in per-pass push constants
    {
        auto& b = current_frame.buffers;

        // Main pass push constants — all BDA addresses
        m_obj_config.bdag_camera = b.dynamic_data.device_address();
        m_obj_config.bdag_objects = b.objects.device_address();
        m_obj_config.bdag_directional_lights = b.directional_lights.device_address();
        m_obj_config.bdag_universal_lights = b.universal_lights.device_address();
        m_obj_config.bdag_cluster_counts = b.cluster_counts.device_address();
        m_obj_config.bdag_cluster_indices = b.cluster_indices.device_address();
        m_obj_config.bdag_cluster_config = b.cluster_config.device_address();
        m_obj_config.bdag_instance_slots = b.instance_slots.device_address();
        m_obj_config.bdag_bone_matrices = b.bone_matrices.device_address();
        m_obj_config.bdag_shadow_data = b.shadow_data.device_address();
        m_obj_config.bdag_probe_data = b.probe_data.device_address();
        m_obj_config.bdag_probe_grid = b.probe_grid.device_address();
        // bdaf_material set per-draw in bind_material()

        // Shadow pass push constants
        m_shadow_pc.bdag_objects = b.objects.device_address();
        m_shadow_pc.bdag_instance_slots = b.instance_slots.device_address();
        m_shadow_pc.bdag_shadow_data = b.shadow_data.device_address();

        // Pick pass push constants
        m_pick_pc.bdag_camera = b.dynamic_data.device_address();
        m_pick_pc.bdag_objects = b.objects.device_address();
        m_pick_pc.bdag_instance_slots = b.instance_slots.device_address();
        m_pick_pc.bdag_bone_matrices = b.bone_matrices.device_address();

        // Grid pass push constants
        m_grid_pc.bdag_camera = b.dynamic_data.device_address();
    }
}

frame_state&
vulkan_render::get_current_frame_transfer_data()
{
    return m_frames[glob::glob_state().getr_render_device().get_current_frame_index()];
}

void
vulkan_render::resize(uint32_t width, uint32_t height)
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)(width), (float)(height));
}

vulkan_render_data*
vulkan_render::object_id_under_coordinate(uint32_t x, uint32_t y)
{
    // Source for the copy is the last rendered swapchain image
    auto picking_pass =
        glob::glob_state().getr_vulkan_render_loader().get_render_pass(AID("picking"));
    auto src_image = picking_pass->get_color_images()[0]->image();

    // Create the linear tiled destination image to copy to and to read the memory from

    auto extent = VkExtent3D{m_width, m_height, 1};

    auto image_ci = vk_utils::make_image_create_info(VK_FORMAT_R8G8B8A8_UNORM, 0, extent);
    image_ci.imageType = VK_IMAGE_TYPE_2D;
    image_ci.arrayLayers = 1;
    image_ci.mipLevels = 1;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci.tiling = VK_IMAGE_TILING_LINEAR;
    image_ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // Create the image

    VmaAllocationCreateInfo vma_allocinfo = {};
    vma_allocinfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    vma_allocinfo.requiredFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    auto dst_image = vk_utils::vulkan_image::create(
        glob::glob_state().getr_render_device().get_vma_allocator_provider(),
        image_ci,
        vma_allocinfo);

    auto cmd_buf_ai = vk_utils::make_command_buffer_allocate_info(
        glob::glob_state().getr_render_device().m_upload_context.m_command_pool, 1);

    VkCommandBuffer cmd_buffer;
    vkAllocateCommandBuffers(
        glob::glob_state().getr_render_device().vk_device(), &cmd_buf_ai, &cmd_buffer);

    auto command_buffer_bi = vk_utils::make_command_buffer_begin_info();
    vkBeginCommandBuffer(cmd_buffer, &command_buffer_bi);

    // Transition destination image to transfer destination layout
    vk_utils::make_insert_image_memory_barrier(
        cmd_buffer,
        dst_image.image(),
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    {
        // Otherwise use image copy (requires us to manually flip components)
        VkImageCopy image_copy_region{};
        image_copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_copy_region.srcSubresource.layerCount = 1;
        image_copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_copy_region.dstSubresource.layerCount = 1;
        image_copy_region.extent = extent;

        // Issue the copy command
        vkCmdCopyImage(cmd_buffer,
                       src_image,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       dst_image.image(),
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1,
                       &image_copy_region);
    }

    // Transition destination image to general layout, which is the required layout for mapping the
    // image memory later on
    vk_utils::make_insert_image_memory_barrier(
        cmd_buffer,
        dst_image.image(),
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    glob::glob_state().getr_render_device().flush_command_buffer(
        cmd_buffer, glob::glob_state().getr_render_device().vk_graphics_queue());

    // Map image memory so we can start copying from it

    auto mp = ImGui::GetIO().MousePos;

    auto data = dst_image.map();

    data += (x + y * m_width) * 4;

    uint32_t pixel = 0;

    memcpy(&pixel, data, 4);

    dst_image.unmap();

    auto obj_slot = pixel & 0xFFFFFF;

    if (!obj_slot)
    {
        return nullptr;
    }

    return m_cache.objects.at(obj_slot);
}

void
vulkan_render::schd_update_texture(texture_data* tex)
{
    auto& q = get_current_frame_transfer_data();
    q.uploads.textures_queue.emplace_back(tex);
}

void
vulkan_render::update_bindless_descriptors()
{
    auto& current_frame = get_current_frame_transfer_data();
    auto& textures_queue = current_frame.uploads.textures_queue;

    if (textures_queue.empty())
    {
        return;
    }

    auto device = glob::glob_state().get_render_device();

    std::vector<VkDescriptorImageInfo> image_infos;
    std::vector<VkWriteDescriptorSet> writes;
    image_infos.reserve(textures_queue.get_size());
    writes.reserve(textures_queue.get_size());

    for (auto* tex : textures_queue)
    {
        if (!tex || !tex->image_view)
        {
            continue;
        }

        VkDescriptorImageInfo image_info{};
        image_info.sampler = VK_NULL_HANDLE;  // Sampler comes from static_samplers binding
        image_info.imageView = tex->image_view->vk();
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_infos.push_back(image_info);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_bindless_set;
        write.dstBinding = 1;  // Textures at binding 1 (samplers at binding 0)
        write.dstArrayElement = tex->get_bindless_index();
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo = &image_infos.back();
        writes.push_back(write);
    }

    if (!writes.empty())
    {
        vkUpdateDescriptorSets(
            device->vk_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    textures_queue.clear();
}

}  // namespace render
}  // namespace kryga
