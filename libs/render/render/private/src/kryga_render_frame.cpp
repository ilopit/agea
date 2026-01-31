#include "vulkan_render/kryga_render.h"

#include <tracy/Tracy.hpp>

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/types/vulkan_sampler_data.h"

#include <gpu_types/gpu_generic_constants.h>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <native/native_window.h>

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

    auto device = glob::render_device::get();

    auto r = SDL_GetWindowFlags(glob::native_window::getr().handle());

    // TODO, rework
    if ((SDL_WINDOW_MINIMIZED & r) == SDL_WINDOW_MINIMIZED)
    {
        return;
    }

    device->switch_frame_indeces();
    m_culled_draws = 0;
    m_all_draws = 0;

    auto& current_frame = m_frames[device->get_current_frame_index()];

    // wait until the gpu has finished rendering the last frame. Timeout of 1 second
    {
        ZoneScopedN("Render::WaitForFence");
        VK_CHECK(vkWaitForFences(device->vk_device(), 1, &current_frame.frame->m_render_fence, true,
                                 1000000000));
    }
    VK_CHECK(vkResetFences(device->vk_device(), 1, &current_frame.frame->m_render_fence));

    current_frame.frame->m_dynamic_descriptor_allocator->reset_pools();

    // now that we are sure that the commands finished executing, we can safely reset the command
    // buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(current_frame.frame->m_main_command_buffer, 0));

    // request image from the swapchain
    uint32_t swapchain_image_index = 0U;
    VK_CHECK(vkAcquireNextImageKHR(device->vk_device(), device->swapchain(), 1000000000,
                                   current_frame.frame->m_present_semaphore, nullptr,
                                   &swapchain_image_index));

    // naming it cmd for shorter writing
    auto cmd = current_frame.frame->m_main_command_buffer;

    // begin the command buffer recording. We will use this command buffer exactly once, so we want
    // to let vulkan know that
    auto cmd_begin_info =
        vk_utils::make_command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

    auto width = (uint32_t)glob::native_window::get()->get_size().w;
    auto height = (uint32_t)glob::native_window::get()->get_size().h;

    update_ui(current_frame);
    prepare_draw_resources(current_frame);

    // Set up render context for callbacks
    m_current_frame = &current_frame;
    m_render_graph.set_frame_context(swapchain_image_index, width, height);

    // Build descriptor set for cluster culling using binding table
    // Binding names must match render graph resource names (dyn_ prefix)
    if (is_instanced_mode() && m_cluster_cull_pass &&
        m_cluster_cull_pass->are_bindings_finalized())
    {
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

    // Build descriptor set for frustum culling
    if (is_instanced_mode() && m_frustum_cull_pass && m_gpu_frustum_culling_enabled &&
        m_frustum_cull_pass->are_bindings_finalized())
    {
        m_frustum_cull_pass->begin_frame();
        m_frustum_cull_pass->bind(AID("dyn_frustum_data"), current_frame.buffers.frustum_data);
        m_frustum_cull_pass->bind(AID("dyn_object_buffer"), current_frame.buffers.objects);
        m_frustum_cull_pass->bind(AID("dyn_visible_indices"), current_frame.buffers.visible_indices);
        m_frustum_cull_pass->bind(AID("dyn_cull_output"), current_frame.buffers.cull_output);

        m_frustum_cull_descriptor_set = m_frustum_cull_pass->get_descriptor_set(
            0, *current_frame.frame->m_dynamic_descriptor_allocator);
    }

    // Begin frame - clears binding tracking for validation
    m_render_graph.begin_frame();

    // Bind per-frame buffer resources to the graph (names must match shader bindings)
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
    m_render_graph.bind_buffer(AID("dyn_material_buffer"), current_frame.buffers.materials);

    // Frustum cull buffers (instanced mode only)
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

    // Main pass uses swapchain images (multiple, indexed by swapchain_image_index)
    auto main_images = main_pass->get_color_images();
    m_render_graph.bind_image(AID("swapchain"), *main_images[swapchain_image_index]);

    // UI and picking passes have single render targets (not per-swapchain)
    auto ui_images = ui_pass->get_color_images();
    m_render_graph.bind_image(AID("ui_target"), *ui_images[0]);

    auto picking_images = picking_pass->get_color_images();
    m_render_graph.bind_image(AID("picking_target"), *picking_images[0]);

    // Execute render graph (handles passes in dependency order with auto barriers)
    m_render_graph.execute(cmd, device->get_current_frame_index(), width, height);

    // finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    auto submit = render::vk_utils::make_submit_info(&cmd);
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    submit.pWaitDstStageMask = &wait_stage;

    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &current_frame.frame->m_present_semaphore;

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &current_frame.frame->m_render_semaphore;

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit(device->vk_graphics_queue(), 1, &submit,
                           current_frame.frame->m_render_fence));

    auto present_info = render::vk_utils::make_present_info();

    present_info.pSwapchains = &device->swapchain();
    present_info.swapchainCount = 1;

    present_info.pWaitSemaphores = &current_frame.frame->m_render_semaphore;
    present_info.waitSemaphoreCount = 1;

    present_info.pImageIndices = &swapchain_image_index;

    VK_CHECK(vkQueuePresentKHR(device->vk_graphics_queue(), &present_info));
}

void
vulkan_render::prepare_draw_resources(render::frame_state& current_frame)
{
    ZoneScopedN("Render::PrepareResources");

    // Update bindless texture descriptors for any newly registered textures
    update_bindless_descriptors();

    // Build instance data (batches for instanced path, identity buffer for legacy path)
    // Both paths need the instance_slots buffer populated for shaders to work
    prepare_instance_data(current_frame);

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
        // Upload frustum data for GPU frustum culling
        if (m_frustum_cull_pass && m_gpu_frustum_culling_enabled)
        {
            upload_frustum_data(current_frame);
        }

        if (m_cluster_cull_pass && m_cluster_cull_shader)
        {
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
            // CPU fallback path
            ZoneScopedN("Render::CPUBuildClusters");
            build_light_clusters();
            m_clusters_dirty = false;
            upload_cluster_data(current_frame);
        }
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

    // Bind resources to main pass
    auto* main_pass = get_render_pass(AID("main"));
    KRG_check(main_pass && main_pass->are_bindings_finalized(),
              "Main pass bindings must be finalized");

    main_pass->begin_frame();
    main_pass->bind(AID("dyn_camera_data"), current_frame.buffers.dynamic_data);
    main_pass->bind(AID("dyn_object_buffer"), current_frame.buffers.objects);
    main_pass->bind(AID("dyn_directional_lights_buffer"), current_frame.buffers.directional_lights);
    main_pass->bind(AID("dyn_gpu_universal_light_data"), current_frame.buffers.universal_lights);
    main_pass->bind(AID("dyn_cluster_light_counts"), current_frame.buffers.cluster_counts);
    main_pass->bind(AID("dyn_cluster_light_indices"), current_frame.buffers.cluster_indices);
    main_pass->bind(AID("dyn_cluster_config"), current_frame.buffers.cluster_config);
    main_pass->bind(AID("dyn_instance_slots"), current_frame.buffers.instance_slots);

    m_global_set = main_pass->get_descriptor_set(
        KGPU_global_descriptor_sets, *current_frame.frame->m_dynamic_descriptor_allocator);
    m_objects_set = main_pass->get_descriptor_set(
        KGPU_objects_descriptor_sets, *current_frame.frame->m_dynamic_descriptor_allocator);

    // Bind resources to picking pass
    auto* picking_pass = get_render_pass(AID("picking"));
    KRG_check(picking_pass && picking_pass->are_bindings_finalized(),
              "Picking pass bindings must be finalized");

    picking_pass->begin_frame();
    picking_pass->bind(AID("dyn_camera_data"), current_frame.buffers.dynamic_data);
    picking_pass->bind(AID("dyn_object_buffer"), current_frame.buffers.objects);
    picking_pass->bind(AID("dyn_directional_lights_buffer"),
                       current_frame.buffers.directional_lights);
    picking_pass->bind(AID("dyn_gpu_universal_light_data"), current_frame.buffers.universal_lights);
    picking_pass->bind(AID("dyn_cluster_light_counts"), current_frame.buffers.cluster_counts);
    picking_pass->bind(AID("dyn_cluster_light_indices"), current_frame.buffers.cluster_indices);
    picking_pass->bind(AID("dyn_cluster_config"), current_frame.buffers.cluster_config);
    picking_pass->bind(AID("dyn_instance_slots"), current_frame.buffers.instance_slots);
}

frame_state&
vulkan_render::get_current_frame_transfer_data()
{
    return m_frames[glob::render_device::getr().get_current_frame_index()];
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
    auto picking_pass = glob::vulkan_render_loader::getr().get_render_pass(AID("picking"));
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
        glob::render_device::getr().get_vma_allocator_provider(), image_ci, vma_allocinfo);

    auto cmd_buf_ai = vk_utils::make_command_buffer_allocate_info(
        glob::render_device::getr().m_upload_context.m_command_pool, 1);

    VkCommandBuffer cmd_buffer;
    vkAllocateCommandBuffers(glob::render_device::getr().vk_device(), &cmd_buf_ai, &cmd_buffer);

    auto command_buffer_bi = vk_utils::make_command_buffer_begin_info();
    vkBeginCommandBuffer(cmd_buffer, &command_buffer_bi);

    // Transition destination image to transfer destination layout
    vk_utils::make_insert_image_memory_barrier(
        cmd_buffer, dst_image.image(), 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
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
        vkCmdCopyImage(cmd_buffer, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       dst_image.image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                       &image_copy_region);
    }

    // Transition destination image to general layout, which is the required layout for mapping the
    // image memory later on
    vk_utils::make_insert_image_memory_barrier(
        cmd_buffer, dst_image.image(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    glob::render_device::getr().flush_command_buffer(
        cmd_buffer, glob::render_device::getr().vk_graphics_queue());

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
vulkan_render::mark_texture_dirty(texture_data* tex)
{
    auto& q = get_current_frame_transfer_data();
    q.uploads.textures_queue.emplace_back(tex);
}

void
vulkan_render::update_bindless_descriptors()
{
    auto& current_frame = get_current_frame_transfer_data();
    auto& textures_queue = current_frame.uploads.textures_queue;

    ALOG_INFO("update_bindless_descriptors: {} dirty textures, bindless_set={}",
              textures_queue.get_size(), (void*)m_bindless_set);

    if (textures_queue.empty())
    {
        return;
    }

    auto device = glob::render_device::get();

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
        vkUpdateDescriptorSets(device->vk_device(), static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
    }

    textures_queue.clear();
}

}  // namespace render
}  // namespace kryga
