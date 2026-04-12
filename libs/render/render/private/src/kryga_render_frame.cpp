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

    // Build descriptor set for cluster culling
    KRG_check(m_cluster_cull_pass, "Cluster cull pass required");
    KRG_check(m_cluster_cull_pass->are_bindings_finalized(), "Cluster cull bindings not finalized");

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

    // Build descriptor set for frustum culling
    KRG_check(m_frustum_cull_pass, "Frustum cull pass required");
    KRG_check(m_gpu_frustum_culling_enabled, "GPU frustum culling required");
    KRG_check(m_frustum_cull_pass->are_bindings_finalized(), "Frustum cull bindings not finalized");

    m_frustum_cull_pass->begin_frame();
    m_frustum_cull_pass->bind(AID("dyn_frustum_data"), current_frame.buffers.frustum_data);
    m_frustum_cull_pass->bind(AID("dyn_object_buffer"), current_frame.buffers.objects);
    m_frustum_cull_pass->bind(AID("dyn_visible_indices"), current_frame.buffers.visible_indices);
    m_frustum_cull_pass->bind(AID("dyn_cull_output"), current_frame.buffers.cull_output);

    m_frustum_cull_descriptor_set = m_frustum_cull_pass->get_descriptor_set(
        0, *current_frame.frame->m_dynamic_descriptor_allocator);

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

    m_render_graph.bind_buffer(AID("dyn_frustum_data"), current_frame.buffers.frustum_data);
    m_render_graph.bind_buffer(AID("dyn_visible_indices"), current_frame.buffers.visible_indices);
    m_render_graph.bind_buffer(AID("dyn_cull_output"), current_frame.buffers.cull_output);

    // Bind per-frame image resources
    // All render targets are cleared each frame, so UNDEFINED initial layout is safe.
    auto* main_pass = get_render_pass(AID("main"));
    auto main_images = main_pass->get_color_images();
    m_render_graph.bind_image(
        AID("swapchain"), *main_images[swapchain_image_index], VK_IMAGE_LAYOUT_UNDEFINED);

    auto* ui_pass = get_render_pass(AID("ui"));
    auto ui_images = ui_pass->get_color_images();
    m_render_graph.bind_image(AID("ui_target"), *ui_images[0], VK_IMAGE_LAYOUT_UNDEFINED);

    auto* mask_pass = get_render_pass(AID("selection_mask"));
    auto mask_images = mask_pass->get_color_images();
    m_render_graph.bind_image(
        AID("selection_mask_target"), *mask_images[0], VK_IMAGE_LAYOUT_UNDEFINED);

    // Shadow maps start in SHADER_READ_ONLY_OPTIMAL from init_shadow_resources().
    // Use swapchain_image_index to select correct depth image (triple-buffered).
    for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
    {
        if (m_shadow_passes[c])
        {
            auto& depth_images = m_shadow_passes[c]->get_depth_images();
            if (!depth_images.empty())
            {
                uint32_t idx = swapchain_image_index % depth_images.size();
                m_render_graph.bind_image(AID("shadow_csm_" + std::to_string(c)),
                                          depth_images[idx],
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
                uint32_t idx = swapchain_image_index % front.size();
                m_render_graph.bind_image(AID("shadow_local_" + std::to_string(i)),
                                          front[idx],
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }
        if (m_shadow_local_passes[i * 2 + 1])
        {
            auto& back = m_shadow_local_passes[i * 2 + 1]->get_depth_images();
            if (!back.empty())
            {
                uint32_t idx = swapchain_image_index % back.size();
                m_render_graph.bind_image(AID("shadow_local_back_" + std::to_string(i)),
                                          back[idx],
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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

    // Set lighting enable flags on push constants (read by shaders)
    m_obj_config.enable_directional_light = m_render_config.lighting.directional_enabled ? 1 : 0;
    m_obj_config.enable_local_lights = m_render_config.lighting.local_enabled ? 1 : 0;
    m_obj_config.enable_baked_light = m_render_config.lighting.baked_enabled ? 1 : 0;

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

    // Upload frustum data for GPU frustum culling
    upload_frustum_data(current_frame);

    // GPU cluster culling
    {
        ZoneScopedN("Render::GPUClusterCull");

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

        m_clusters_dirty = false;
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

    // Selection mask pass BDA binding
    auto* mask_pass_ptr = get_render_pass(AID("selection_mask"));
    if (mask_pass_ptr && mask_pass_ptr->are_bindings_finalized())
    {
        mask_pass_ptr->begin_frame();
        mask_pass_ptr->bind(AID("dyn_camera_data"), current_frame.buffers.dynamic_data);
        mask_pass_ptr->bind(AID("dyn_object_buffer"), current_frame.buffers.objects);
        mask_pass_ptr->bind(AID("dyn_instance_slots"), current_frame.buffers.instance_slots);
        mask_pass_ptr->bind(AID("dyn_bone_matrices"), current_frame.buffers.bone_matrices);

        KRG_check(static_cast<const render_pass*>(mask_pass_ptr)->bindings().validate_bda_bound(),
                  "Selection mask pass: not all BDA resources bound this frame");
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
    // Rebuild BVH if objects changed
    // TODO, Move to model
    if (m_object_bvh_dirty)
    {
        std::vector<bvh_object_entry> entries;

        auto collect = [&](const auto& queue_map)
        {
            for (const auto& [qid, container] : queue_map)
            {
                for (auto* obj : container)
                {
                    if (!obj->renderable)
                    {
                        continue;
                    }

                    // Build world-space AABB from unit local bounds + model matrix
                    glm::vec3 local_min(-0.5f);
                    glm::vec3 local_max(0.5f);

                    glm::vec3 world_min(std::numeric_limits<float>::max());
                    glm::vec3 world_max(std::numeric_limits<float>::lowest());

                    for (int c = 0; c < 8; ++c)
                    {
                        glm::vec3 corner((c & 1) ? local_max.x : local_min.x,
                                         (c & 2) ? local_max.y : local_min.y,
                                         (c & 4) ? local_max.z : local_min.z);
                        glm::vec3 world_corner =
                            glm::vec3(obj->gpu_data.model * glm::vec4(corner, 1.0f));
                        world_min = glm::min(world_min, world_corner);
                        world_max = glm::max(world_max, world_corner);
                    }

                    // Enforce minimum thickness on all axes.
                    // Billboards and flat quads have zero depth on one axis —
                    // inflate to make them clickable. Editor-only objects
                    // (gizmo icons) get a larger minimum for easier picking.
                    bool is_editor = (obj->layer_flags & render::LAYER_EDITOR_ONLY) != 0;
                    float min_extent = is_editor ? 1.0f : 0.1f;

                    glm::vec3 center = (world_min + world_max) * 0.5f;
                    glm::vec3 half = (world_max - world_min) * 0.5f;
                    half = glm::max(half, glm::vec3(min_extent));
                    world_min = center - half;
                    world_max = center + half;

                    entries.push_back({.aabb_min = world_min,
                                       .aabb_max = world_max,
                                       .user_id = obj->slot(),
                                       .user_data = obj});
                }
            }
        };

        collect(m_default_render_object_queue);
        collect(m_outline_render_object_queue);
        collect(m_debug_render_object_queue);

        for (auto* obj : m_transparent_render_object_queue)
        {
            if (!obj->renderable)
            {
                continue;
            }

            float r = obj->gpu_data.bounding_radius;
            glm::vec3 local_min(-r);
            glm::vec3 local_max(r);

            glm::vec3 world_min(std::numeric_limits<float>::max());
            glm::vec3 world_max(std::numeric_limits<float>::lowest());

            for (int c = 0; c < 8; ++c)
            {
                glm::vec3 corner((c & 1) ? local_max.x : local_min.x,
                                 (c & 2) ? local_max.y : local_min.y,
                                 (c & 4) ? local_max.z : local_min.z);
                glm::vec3 world_corner = glm::vec3(obj->gpu_data.model * glm::vec4(corner, 1.0f));
                world_min = glm::min(world_min, world_corner);
                world_max = glm::max(world_max, world_corner);
            }

            entries.push_back({.aabb_min = world_min,
                               .aabb_max = world_max,
                               .user_id = obj->slot(),
                               .user_data = obj});
        }

        m_object_bvh.build(entries.data(), static_cast<uint32_t>(entries.size()));
        m_object_bvh_dirty = false;
    }

    // Cast ray — nearest AABB hit wins
    auto inv_view = glm::inverse(m_camera_data.view);
    auto r =
        object_bvh::screen_to_ray(x, y, m_width, m_height, m_camera_data.inv_projection, inv_view);

    raycast_hit hit;
    if (m_object_bvh.raycast(r, hit))
    {
        return static_cast<vulkan_render_data*>(hit.user_data);
    }

    return nullptr;
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
