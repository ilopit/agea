#include "vulkan_render/kryga_render.h"
#include "vulkan_render/vulkan_loaders/vulkan_compute_shader_loader.h"

#include <tracy/Tracy.hpp>

#include <cmath>

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/vk_descriptors.h"
#include "vulkan_render/utils/vulkan_converters.h"
#include "vulkan_render/utils/vulkan_initializers.h"
#include "vulkan_render/types/vulkan_texture_data.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_sampler_data.h"
#include "vulkan_render/types/vulkan_shader_data.h"
#include "vulkan_render/types/vulkan_mesh_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/types/vulkan_render_data.h"

#include <gpu_types/gpu_generic_constants.h>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <SDL_events.h>

#include <native/native_window.h>

#include <utils/kryga_log.h>
#include <utils/clock.h>
#include <utils/dynamic_object.h>
#include <utils/dynamic_object_builder.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_vulkan.h>

#include <resource_locator/resource_locator.h>
#include <global_state/global_state.h>

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

void*
ensure_buffer_capacity_and_map(vk_utils::vulkan_buffer& buffer,
                               size_t required_size,
                               const char* name)
{
    KRG_check(required_size, "Should never happen");

    if (required_size >= buffer.get_alloc_size())
    {
        auto old_buffer = std::move(buffer);

        buffer = glob::render_device::getr().create_buffer(
            required_size * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        ALOG_INFO("Reallocating {0} buffer {1} => {2}", name, old_buffer.get_alloc_size(),
                  buffer.get_alloc_size());

        old_buffer.begin();
        buffer.begin();

        memcpy(buffer.get_data(), old_buffer.get_data(), old_buffer.get_alloc_size());

        old_buffer.end();
    }
    else
    {
        buffer.begin();
    }

    return buffer.allocate_data((uint32_t)required_size);
}

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
}

void
vulkan_render::deinit()
{
    m_frames.clear();
}

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

    // Build descriptor set for cluster culling (needed before execute)
    if (m_use_clustered_lighting && m_use_gpu_cluster_cull && m_cluster_cull_shader)
    {
        build_cluster_cull_descriptor_set(current_frame);
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
    m_render_graph.bind_buffer(AID("dyn_material_buffer"), current_frame.buffers.materials);

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
    m_render_graph.execute(cmd);

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
vulkan_render::draw_objects(render::frame_state& current_frame)
{
    ZoneScopedN("Render::DrawObjects");

    auto cmd = current_frame.frame->m_main_command_buffer;

    auto device = glob::render_device::get();

    // DEFAULT
    for (auto& r : m_default_render_object_queue)
    {
        draw_objects_queue(r.second, cmd, current_frame, false);
    }

    // OUTLINE
    for (auto& r : m_outline_render_object_queue)
    {
        draw_objects_queue(r.second, cmd, current_frame, true);
    }

    pipeline_ctx pctx{};
    bind_material(cmd, m_outline_mat, current_frame, pctx, false);

    for (auto& r : m_outline_render_object_queue)
    {
        draw_same_pipeline_objects_queue(cmd, pctx, r.second, false);
    }

    // TRANSPARENT
    if (!m_transparent_render_object_queue.empty())
    {
        update_transparent_objects_queue();
        draw_multi_pipeline_objects_queue(m_transparent_render_object_queue, cmd, current_frame);
    }

    // Draw UI
    auto m = glob::vulkan_render_loader::getr().get_mesh_data(AID("plane_mesh"));

    pctx = {};
    bind_material(cmd, m_ui_target_mat, current_frame, pctx, false, false);
    bind_mesh(cmd, m);

    if (!m->has_indices())
    {
        vkCmdDraw(cmd, m->vertices_size(), 1, 0, 0);
    }
    else
    {
        vkCmdDrawIndexed(cmd, m->indices_size(), 1, 0, 0, 0);
    }
}

void
vulkan_render::prepare_draw_resources(render::frame_state& current_frame)
{
    ZoneScopedN("Render::PrepareResources");

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

    if (m_use_clustered_lighting)
    {
        if (m_use_gpu_cluster_cull && m_cluster_cull_shader)
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
        // Rebuild per-object light grid when lights changed
        if (m_light_grid_dirty)
        {
            ZoneScopedN("Render::RebuildLightGrid");
            rebuild_light_grid();
            m_light_grid_dirty = false;
        }
    }

    // Bind resources to main pass
    auto* main_pass = get_render_pass(AID("main"));
    KRG_check(main_pass && main_pass->are_bindings_finalized(), "Main pass bindings must be finalized");

    main_pass->begin_frame();
    main_pass->bind(AID("dyn_camera_data"), current_frame.buffers.dynamic_data);
    main_pass->bind(AID("dyn_object_buffer"), current_frame.buffers.objects);
    main_pass->bind(AID("dyn_directional_lights_buffer"), current_frame.buffers.directional_lights);
    main_pass->bind(AID("dyn_gpu_universal_light_data"), current_frame.buffers.universal_lights);
    main_pass->bind(AID("dyn_cluster_light_counts"), current_frame.buffers.cluster_counts);
    main_pass->bind(AID("dyn_cluster_light_indices"), current_frame.buffers.cluster_indices);
    main_pass->bind(AID("dyn_cluster_config"), current_frame.buffers.cluster_config);

    m_global_set =
        main_pass->get_descriptor_set(KGPU_global_descriptor_sets,
                                      *current_frame.frame->m_dynamic_descriptor_allocator);
    m_objects_set =
        main_pass->get_descriptor_set(KGPU_objects_descriptor_sets,
                                      *current_frame.frame->m_dynamic_descriptor_allocator);

    // Bind resources to picking pass
    auto* picking_pass = get_render_pass(AID("picking"));
    KRG_check(picking_pass && picking_pass->are_bindings_finalized(),
              "Picking pass bindings must be finalized");

    picking_pass->begin_frame();
    picking_pass->bind(AID("dyn_camera_data"), current_frame.buffers.dynamic_data);
    picking_pass->bind(AID("dyn_object_buffer"), current_frame.buffers.objects);
    picking_pass->bind(AID("dyn_directional_lights_buffer"), current_frame.buffers.directional_lights);
    picking_pass->bind(AID("dyn_gpu_universal_light_data"), current_frame.buffers.universal_lights);
    picking_pass->bind(AID("dyn_cluster_light_counts"), current_frame.buffers.cluster_counts);
    picking_pass->bind(AID("dyn_cluster_light_indices"), current_frame.buffers.cluster_indices);
    picking_pass->bind(AID("dyn_cluster_config"), current_frame.buffers.cluster_config);
}

void
vulkan_render::upload_obj_data(render::frame_state& frame)
{
    const auto total_size = m_cache.objects.get_size() * sizeof(gpu::object_data);

    auto* data = (gpu::object_data*)ensure_buffer_capacity_and_map(frame.buffers.objects,
                                                                   total_size, "objects");
    KRG_check(data, "Should never happen");

    upload_gpu_object_data(data);
    frame.buffers.objects.end();
}

void
vulkan_render::upload_universal_light_data(render::frame_state& frame)
{
    const auto total_size = m_cache.universal_lights.get_size() * sizeof(gpu::universal_light_data);

    auto* data = (gpu::universal_light_data*)ensure_buffer_capacity_and_map(
        frame.buffers.universal_lights, total_size, "universal lights");
    KRG_check(data, "Should never happen");

    upload_gpu_universal_light_data(data);
    frame.buffers.universal_lights.end();
}

void
vulkan_render::upload_directional_light_data(render::frame_state& frame)
{
    const auto total_size =
        m_cache.directional_lights.get_size() * sizeof(gpu::directional_light_data);

    auto* data = (gpu::directional_light_data*)ensure_buffer_capacity_and_map(
        frame.buffers.directional_lights, total_size, "directional lights");

    KRG_check(data, "Should never happen");

    upload_gpu_directional_light_data(data);
    frame.buffers.directional_lights.end();
}

void
vulkan_render::upload_material_data(render::frame_state& frame)
{
    auto total_size = m_materials_layout.calc_new_size();

    bool reallocated = false;

    vk_utils::vulkan_buffer old_buffer_tb;

    if (total_size >= frame.buffers.materials.get_alloc_size())
    {
        old_buffer_tb = std::move(frame.buffers.materials);

        frame.buffers.materials = glob::render_device::getr().create_buffer(
            total_size * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        ALOG_INFO("Reallocating material buffer {0} => {1}", old_buffer_tb.get_alloc_size(),
                  frame.buffers.materials.get_alloc_size());

        reallocated = true;
    }

    if (reallocated)
    {
        old_buffer_tb.begin();
    }

    frame.buffers.materials.begin();

    for (auto i = 0; i < m_materials_layout.get_segments_size(); ++i)
    {
        auto& s = m_materials_layout.at(i);

        auto src_offset = s.offset;
        auto size = s.get_allocated_size();

        m_materials_layout.update_segment(i);

        if (m_materials_layout.dirty_layout())
        {
            auto dst_offset = s.offset;

            if (reallocated)
            {
                memcpy(frame.buffers.materials.get_data() + dst_offset,
                       old_buffer_tb.get_data() + src_offset, size);
            }
            else
            {
                memmove(frame.buffers.materials.get_data() + dst_offset,
                        old_buffer_tb.get_data() + src_offset, size);
            }
        }
    }

    if (reallocated)
    {
        old_buffer_tb.end();
    }

    auto mat_begin = frame.buffers.materials.get_data();

    for (int i = 0; i < m_materials_layout.get_segments_size(); ++i)
    {
        auto& sm = m_materials_layout.at(i);
        auto& mat_set_queue = frame.uploads.materials_queue_set[sm.index];

        upload_gpu_materials_data(mat_begin + sm.offset, mat_set_queue);
    }

    frame.buffers.materials.end();
    m_materials_layout.reset_dirty_layout();
}

void
vulkan_render::draw_multi_pipeline_objects_queue(render_line_container& r,
                                                 VkCommandBuffer cmd,
                                                 render::frame_state& current_frame)
{
    mesh_data* cur_mesh = nullptr;

    pipeline_ctx pctx{};

    for (auto& obj : r)
    {
        ++m_all_draws;
        // Frustum culling
        if (!m_frustum.is_sphere_visible(obj->gpu_data.obj_pos, obj->bounding_radius))
        {
            ++m_culled_draws;
            continue;
        }

        if (pctx.cur_material_type_idx != obj->material->gpu_type_idx())
        {
            bind_material(cmd, obj->material, current_frame, pctx);
        }
        else if (pctx.cur_material_idx != obj->material->gpu_idx())
        {
            pctx.cur_material_idx = obj->material->gpu_idx();

            if (obj->material->has_textures())
            {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pctx.pipeline_layout,
                                        KGPU_textures_descriptor_sets, 1,
                                        &obj->material->get_textures_ds(), 0, nullptr);
            }
        }

        if (cur_mesh != obj->mesh)
        {
            cur_mesh = obj->mesh;
            bind_mesh(cmd, cur_mesh);
        }

        draw_object(cmd, pctx, obj);
    }
}

void
vulkan_render::draw_objects_queue(render_line_container& r,
                                  VkCommandBuffer cmd,
                                  render::frame_state& current_frame,
                                  bool outlined)

{
    pipeline_ctx pctx{};

    if (!r.empty())
    {
        bind_material(cmd, r.front()->material, current_frame, pctx, outlined);
    }

    draw_same_pipeline_objects_queue(cmd, pctx, r);
}

void
vulkan_render::draw_same_pipeline_objects_queue(VkCommandBuffer cmd,
                                                const pipeline_ctx& pctx,
                                                const render_line_container& r,
                                                bool rebind_images)
{
    mesh_data* cur_mesh = nullptr;
    uint32_t cur_material_idx = pctx.cur_material_idx;

    for (auto& obj : r)
    {
        ++m_all_draws;
        // Frustum culling
        if (!m_frustum.is_sphere_visible(obj->gpu_data.obj_pos, obj->bounding_radius))
        {
            ++m_culled_draws;
            continue;
        }

        if (rebind_images && cur_material_idx != obj->material->gpu_idx())
        {
            cur_material_idx = obj->material->gpu_idx();

            if (obj->material->has_textures())
            {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pctx.pipeline_layout,
                                        KGPU_textures_descriptor_sets, 1,
                                        &obj->material->get_textures_ds(), 0, nullptr);
            }
        }

        if (cur_mesh != obj->mesh)
        {
            cur_mesh = obj->mesh;
            bind_mesh(cmd, cur_mesh);
        }

        draw_object(cmd, pctx, obj);
    }
}

void
vulkan_render::draw_object(VkCommandBuffer cmd,
                           const pipeline_ctx& pctx,
                           const render::vulkan_render_data* obj)
{
    // Set directional light (global)
    m_obj_config.directional_light_id =
        m_cache.directional_lights.get_size() > 0 ? m_cache.directional_lights.at(0)->slot() : 0;

    // Set lighting mode
    m_obj_config.use_clustered_lighting = m_use_clustered_lighting ? 1 : 0;

    if (!m_use_clustered_lighting)
    {
        // Per-object light grid path: query lights affecting this object
        m_obj_config.local_lights_size =
            m_light_grid.query_lights(obj->gpu_data.obj_pos, obj->bounding_radius,
                                      m_obj_config.local_light_ids, KGPU_max_lights_per_object);
    }
    else
    {
        m_obj_config.local_lights_size = 0;
    }

    auto cur_mesh = obj->mesh;
    m_obj_config.material_id = obj->material->gpu_idx();

    constexpr auto range = sizeof(gpu::push_constants);
    vkCmdPushConstants(cmd, pctx.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, range,
                       &m_obj_config);

    // we can now draw
    if (!obj->mesh->has_indices())
    {
        vkCmdDraw(cmd, cur_mesh->vertices_size(), 1, 0, obj->slot());
    }
    else
    {
        vkCmdDrawIndexed(cmd, cur_mesh->indices_size(), 1, 0, 0, obj->slot());
    }
}

void
vulkan_render::bind_mesh(VkCommandBuffer cmd, mesh_data* cur_mesh)
{
    KRG_check(cur_mesh, "Should not be null");

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &cur_mesh->m_vertex_buffer.buffer(), &offset);

    if (cur_mesh->has_indices())
    {
        vkCmdBindIndexBuffer(cmd, cur_mesh->m_index_buffer.buffer(), 0, VK_INDEX_TYPE_UINT32);
    }
}

void
vulkan_render::bind_material(VkCommandBuffer cmd,
                             material_data* cur_material,
                             render::frame_state& current_frame,
                             pipeline_ctx& ctx,
                             bool outline,
                             bool object)
{
    auto pipeline = outline ? cur_material->get_shader_effect()->m_with_stencil_pipeline
                            : cur_material->get_shader_effect()->m_pipeline;
    ctx.pipeline_layout = cur_material->get_shader_effect()->m_pipeline_layout;
    ctx.cur_material_idx = cur_material->gpu_idx();
    ctx.cur_material_type_idx = cur_material->gpu_type_idx();

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    const uint32_t dummy_offset[] = {0, 0, 0, 0, 0, 0};

    if (object)
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline_layout,
                                KGPU_objects_descriptor_sets, 1, &m_objects_set,
                                KGPU_objects_max_binding + 1, dummy_offset);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline_layout,
                                KGPU_global_descriptor_sets, 1, &m_global_set,
                                current_frame.buffers.dynamic_data.get_dyn_offsets_count(),
                                current_frame.buffers.dynamic_data.get_dyn_offsets_ptr());
    }

    if (cur_material->has_gpu_data())
    {
        auto& sm = m_materials_layout.at(cur_material->gpu_idx());
        VkDescriptorBufferInfo mat_buffer_info{.buffer = current_frame.buffers.materials.buffer(),
                                               .offset = sm.offset,
                                               .range = sm.get_allocated_size()};

        VkDescriptorSet mat_data_set{};
        vk_utils::descriptor_builder::begin(
            glob::render_device::getr().descriptor_layout_cache(),
            current_frame.frame->m_dynamic_descriptor_allocator.get())
            .bind_buffer(0, &mat_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                         VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(mat_data_set);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline_layout,
                                KGPU_materials_descriptor_sets, 1, &mat_data_set, 1, dummy_offset);
    }

    if (cur_material->has_textures())
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline_layout,
                                KGPU_textures_descriptor_sets, 1, &cur_material->get_textures_ds(),
                                0, nullptr);
    }
}

void
vulkan_render::push_config(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout, uint32_t mat_id)
{
    m_obj_config.directional_light_id = 0U;
}

void
vulkan_render::schedule_to_drawing(render::vulkan_render_data* obj_data)
{
    KRG_check(obj_data, "Should be always valid");

    if (obj_data->outlined)
    {
        KRG_check(obj_data->queue_id != "transparent", "Not supported!");

        m_outline_render_object_queue[obj_data->queue_id].emplace_back(obj_data);

        return;
    }

    if (obj_data->queue_id == "transparent")
    {
        m_transparent_render_object_queue.emplace_back(obj_data);
    }
    else
    {
        m_default_render_object_queue[obj_data->queue_id].emplace_back(obj_data);
    }
}

void
vulkan_render::reschedule_to_drawing(render::vulkan_render_data* obj_data)
{
    remove_from_drawing(obj_data);
    schedule_to_drawing(obj_data);
}

void
vulkan_render::remove_from_drawing(render::vulkan_render_data* obj_data)
{
    KRG_check(obj_data, "Should be always valid");

    {
        KRG_check(obj_data->queue_id != "transparent", "Not supported!");

        const std::string id = obj_data->queue_id;

        auto& bucket = m_outline_render_object_queue[id];

        auto itr = bucket.find(obj_data);
        if (itr != bucket.end())
        {
            bucket.swap_and_remove(itr);

            if (bucket.get_size() == 0)
            {
                ALOG_TRACE("Dropping old queue");
                m_outline_render_object_queue.erase(id);
            }
        }
    }

    if (obj_data->queue_id == "transparent")
    {
        auto itr = m_transparent_render_object_queue.find(obj_data);

        m_transparent_render_object_queue.swap_and_remove(itr);
    }
    else
    {
        const std::string id = obj_data->queue_id;

        auto& bucket = m_default_render_object_queue[id];

        auto itr = bucket.find(obj_data);
        if (itr != bucket.end())
        {
            bucket.swap_and_remove(itr);

            if (bucket.get_size() == 0)
            {
                ALOG_TRACE("Dropping old queue");
                m_default_render_object_queue.erase(id);
            }
        }
    }
}

void
vulkan_render::add_material(render::material_data* mat_data)
{
    auto& mat_id = mat_data->get_type_id();

    auto segment = m_materials_layout.find(mat_id);

    if (!segment)
    {
        segment = m_materials_layout.add(mat_id, mat_data->get_gpu_data().size(),
                                         INITIAL_MATERIAL_SEGMENT_RANGE_SIZE);

        for (auto& q : m_frames)
        {
            while (segment->index >= q.uploads.materials_queue_set.get_size())
            {
                q.uploads.materials_queue_set.emplace_back();
            }
        }
    }
    mat_data->set_indexes(segment->alloc_id(), segment->index);
}

void
vulkan_render::drop_material(render::material_data* mat_data)
{
    auto& mat_id = mat_data->get_type_id();
    auto segment = m_materials_layout.find(mat_id);

    if (segment)
    {
        segment->release_id(mat_data->gpu_idx());
        mat_data->invalidate_gpu_indexes();
    }
}

void
vulkan_render::schedule_material_data_gpu_upload(render::material_data* md)
{
    for (auto& q : m_frames)
    {
        q.uploads.materials_queue_set[md->gpu_type_idx()].emplace_back(md);
        q.uploads.has_pending_materials = true;
    }
}

void
vulkan_render::schedule_game_data_gpu_upload(render::vulkan_render_data* obj_date)
{
    for (auto& q : m_frames)
    {
        q.uploads.objects_queue.emplace_back(obj_date);
    }
}

void
vulkan_render::schedule_directional_light_data_gpu_upload(render::vulkan_directional_light_data* ld)
{
    for (auto& q : m_frames)
    {
        q.uploads.directional_light_queue.emplace_back(ld);
    }
}

void
vulkan_render::schedule_universal_light_data_gpu_upload(render::vulkan_universal_light_data* ld)
{
    m_clusters_dirty = true;
    m_light_grid_dirty = true;
    for (auto& q : m_frames)
    {
        q.uploads.universal_light_queue.emplace_back(ld);
    }
}

void
vulkan_render::clear_upload_queue()
{
    for (auto& q : m_frames)
    {
        q.uploads.clear_all();
    }
}

void
vulkan_render::upload_gpu_object_data(gpu::object_data* object_SSBO)
{
    auto& to_update = get_current_frame_transfer_data().uploads.objects_queue;

    if (to_update.empty())
    {
        return;
    }

    for (auto obj : to_update)
    {
        object_SSBO[obj->slot()] = obj->gpu_data;
    }
}

void
vulkan_render::upload_gpu_universal_light_data(gpu::universal_light_data* object_SSBO)
{
    auto& to_update = get_current_frame_transfer_data().uploads.universal_light_queue;

    if (to_update.empty())
    {
        return;
    }

    for (auto obj : to_update)
    {
        obj->gpu_data.slot = obj->slot();  // Ensure slot is synced for GPU access
        object_SSBO[obj->slot()] = obj->gpu_data;
    }
}

void
vulkan_render::upload_gpu_directional_light_data(gpu::directional_light_data* object_SSBO)
{
    auto& to_update = get_current_frame_transfer_data().uploads.directional_light_queue;

    if (to_update.empty())
    {
        return;
    }

    for (auto obj : to_update)
    {
        object_SSBO[obj->slot()] = obj->gpu_data;
    }
}

void
vulkan_render::upload_gpu_materials_data(uint8_t* ssbo_data, materials_update_queue& mats_to_update)
{
    if (mats_to_update.empty())
    {
        return;
    }

    for (auto& m : mats_to_update)
    {
        auto dst_ptr = ssbo_data + m->gpu_idx() * m->get_gpu_data().size();
        memcpy(dst_ptr, m->get_gpu_data().data(), m->get_gpu_data().size());
    }

    mats_to_update.clear();
}

frame_state&
vulkan_render::get_current_frame_transfer_data()
{
    return m_frames[glob::render_device::getr().get_current_frame_index()];
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

    // UI pass doesn't use the standard binding table (has its own simple setup)
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

void
vulkan_render::update_transparent_objects_queue()
{
    for (auto& obj : m_transparent_render_object_queue)
    {
        obj->distance_to_camera = glm::length(obj->gpu_data.obj_pos - m_camera_data.position);
    }

    std::sort(m_transparent_render_object_queue.begin(), m_transparent_render_object_queue.end(),
              [](render::vulkan_render_data* l, render::vulkan_render_data* r)
              { return l->distance_to_camera > r->distance_to_camera; });
}

void
vulkan_render::prepare_ui_resources()
{
    ImGuiIO& io = ImGui::GetIO();

    // Create font texture
    auto path =
        glob::glob_state().get_resource_locator()->resource(category::fonts, "Roboto-Medium.ttf");
    auto f = path.str();

    auto f_normal = io.Fonts->AddFontFromFileTTF(f.c_str(), 28.0f);
    auto f_big = io.Fonts->AddFontFromFileTTF(f.c_str(), 33.0f);

    glob::vulkan_render_loader::getr().create_font(AID("normal"), f_normal);
    glob::vulkan_render_loader::getr().create_font(AID("big"), f_big);

    int tex_width = 0, tex_height = 0;
    unsigned char* font_data = nullptr;
    io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

    auto size = tex_width * tex_height * 4 * sizeof(char);

    kryga::utils::buffer image_raw_buffer;
    image_raw_buffer.resize(size);
    memcpy(image_raw_buffer.data(), font_data, size);

    m_ui_txt = glob::vulkan_render_loader::getr().create_texture(AID("font"), image_raw_buffer,
                                                                 tex_width, tex_height);

    auto ui_pass = glob::vulkan_render_loader::getr().get_render_pass(AID("ui"));
    m_ui_target_txt = glob::vulkan_render_loader::getr().create_texture(
        AID("ui_copy_txt"), ui_pass->get_color_images()[0], ui_pass->get_color_image_views()[0]);
}

void
vulkan_render::prepare_ui_pipeline()
{
    auto path = glob::glob_state().get_resource_locator()->resource(
        category::packages, "base.apkg/class/shader_effects/ui");

    {
        kryga::utils::buffer vert, frag;

        auto vert_path = path / "se_uioverlay.vert";
        kryga::utils::buffer::load(vert_path, vert);

        auto frag_path = path / "se_uioverlay.frag";
        kryga::utils::buffer::load(frag_path, frag);

        auto layout = render::gpu_dynobj_builder()
                          .set_id(AID("interface"))
                          .add_field(AID("in_pos"), kryga::render::gpu_type::g_vec2, 1)
                          .add_field(AID("in_UV"), kryga::render::gpu_type::g_vec2, 1)
                          .add_field(AID("in_color"), kryga::render::gpu_type::g_color, 1)
                          .finalize();

        auto ui_pass = glob::vulkan_render_loader::getr().get_render_pass(AID("ui"));

        shader_effect_create_info se_ci;
        se_ci.vert_buffer = &vert;
        se_ci.frag_buffer = &frag;
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = true;
        se_ci.alpha = alpha_mode::ui;
        se_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;
        se_ci.expected_input_vertex_layout = std::move(layout);

        ui_pass->create_shader_effect(AID("se_ui"), se_ci, m_ui_se);

        std::vector<texture_sampler_data> samples(1);
        samples.front().texture = m_ui_txt;
        samples.front().slot = 0;

        m_ui_mat = glob::vulkan_render_loader::getr().create_material(
            AID("mat_ui"), AID("ui"), samples, *m_ui_se, utils::dynobj{});
    }
    {
        kryga::utils::buffer vert, frag;

        auto vert_path = path / "se_upload.vert";
        kryga::utils::buffer::load(vert_path, vert);

        auto frag_path = path / "se_upload.frag";
        kryga::utils::buffer::load(frag_path, frag);

        auto main_pass = glob::vulkan_render_loader::getr().get_render_pass(AID("main"));

        shader_effect_create_info se_ci;
        se_ci.vert_buffer = &vert;
        se_ci.frag_buffer = &frag;
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = false;
        se_ci.alpha = alpha_mode::ui;
        se_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;

        main_pass->create_shader_effect(AID("se_ui_copy"), se_ci, m_ui_copy_se);

        std::vector<texture_sampler_data> samples(1);
        samples.front().texture = m_ui_target_txt;
        samples.front().slot = 0;

        m_ui_target_mat = glob::vulkan_render_loader::getr().create_material(
            AID("mat_ui_copy"), AID("ui_copy"), samples, *m_ui_copy_se, utils::dynobj{});
    }
}

void
vulkan_render::update_ui(frame_state& fs)
{
    auto device = glob::render_device::get();
    ImDrawData* im_draw_data = ImGui::GetDrawData();

    if (!im_draw_data)
    {
        return;
    };

    // Note: Alignment is done inside buffer creation
    VkDeviceSize vertex_buffer_size = im_draw_data->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize index_buffer_size = im_draw_data->TotalIdxCount * sizeof(ImDrawIdx);

    // Update buffers only if vertex or index count has been changed compared to current
    // buffer size
    if ((vertex_buffer_size == 0) || (index_buffer_size == 0))
    {
        return;
    }

    // Vertex buffer
    if ((fs.ui.vertex_count != im_draw_data->TotalVtxCount) ||
        (fs.ui.index_count != im_draw_data->TotalIdxCount))
    {
        fs.ui.vertex_count = im_draw_data->TotalVtxCount;
        fs.ui.index_count = im_draw_data->TotalIdxCount;

        VkBufferCreateInfo staging_buffer_ci = {};
        staging_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        staging_buffer_ci.pNext = nullptr;
        // this is the total size, in bytes, of the buffer we are allocating

        staging_buffer_ci.size = vertex_buffer_size;
        staging_buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        VmaAllocationCreateInfo vma_ci = {};
        vma_ci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        fs.ui.vertex_buffer = vk_utils::vulkan_buffer::create(staging_buffer_ci, vma_ci);

        staging_buffer_ci.size = index_buffer_size;
        staging_buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        fs.ui.index_buffer = vk_utils::vulkan_buffer::create(staging_buffer_ci, vma_ci);
    }

    // Upload data
    fs.ui.vertex_buffer.begin();
    fs.ui.index_buffer.begin();

    auto vtx_dst = (ImDrawVert*)fs.ui.vertex_buffer.allocate_data((uint32_t)vertex_buffer_size);
    auto idx_dst = (ImDrawIdx*)fs.ui.index_buffer.allocate_data((uint32_t)index_buffer_size);

    for (int n = 0; n < im_draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = im_draw_data->CmdLists[n];
        memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += cmd_list->VtxBuffer.Size;
        idx_dst += cmd_list->IdxBuffer.Size;
    }

    fs.ui.vertex_buffer.end();
    fs.ui.index_buffer.end();

    fs.ui.vertex_buffer.flush();
    fs.ui.index_buffer.flush();
}

void
vulkan_render::draw_ui(frame_state& fs)
{
    auto im_draw_data = ImGui::GetDrawData();

    if ((!im_draw_data) || (im_draw_data->CmdListsCount == 0))
    {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();

    VkViewport viewport{};
    viewport.width = io.DisplaySize.x;
    viewport.height = io.DisplaySize.y;
    viewport.minDepth = 0.;
    viewport.maxDepth = 1.f;

    VkRect2D scissor{};
    scissor.extent.width = (uint32_t)io.DisplaySize.x;
    scissor.extent.height = (uint32_t)io.DisplaySize.y;
    scissor.offset.x = 0;
    scissor.offset.y = 0;

    auto cmd = fs.frame->m_main_command_buffer;

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ui_se->m_pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ui_se->m_pipeline_layout, 0, 1,
                            &m_ui_mat->get_textures_ds(), 0, nullptr);

    m_ui_push_constants.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
    m_ui_push_constants.translate = glm::vec2(-1.0f);

    vkCmdPushConstants(cmd, m_ui_se->m_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(ui_push_constants), &m_ui_push_constants);

    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &fs.ui.vertex_buffer.buffer(), offsets);
    vkCmdBindIndexBuffer(cmd, fs.ui.index_buffer.buffer(), 0, VK_INDEX_TYPE_UINT16);

    int32_t vertex_offset = 0;
    int32_t index_offset = 0;
    for (int32_t i = 0; i < im_draw_data->CmdListsCount; i++)
    {
        const ImDrawList* cmd_list = im_draw_data->CmdLists[i];
        for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
            VkRect2D scissorRect;
            scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
            scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
            scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
            scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
            vkCmdSetScissor(fs.frame->m_main_command_buffer, 0, 1, &scissorRect);
            vkCmdDrawIndexed(fs.frame->m_main_command_buffer, pcmd->ElemCount, 1, index_offset,
                             vertex_offset, 0);
            index_offset += pcmd->ElemCount;
        }
        vertex_offset += cmd_list->VtxBuffer.Size;
    }
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

void
vulkan_render::build_light_clusters()
{
    KRG_check(m_cluster_grid.is_initialized(), "Should always be here");

    // Early-out: no universal lights means no cluster work needed
    if (m_cache.universal_lights.get_size() == 0)
    {
        m_cluster_grid.clear();
        return;
    }

    // Build light info list from cache (skip invalid/freed lights)
    std::vector<cluster_light_info> lights;
    lights.reserve(m_cache.universal_lights.get_actual_size());

    for (uint32_t i = 0; i < m_cache.universal_lights.get_size(); ++i)
    {
        auto* light = m_cache.universal_lights.at(i);
        if (!light->is_valid())
            continue;
        // Add small margin for cluster assignment to avoid edge cases
        lights.push_back({light->slot(), light->gpu_data.position, light->gpu_data.radius * 1.05f});
    }

    // Compute view and projection matrices for cluster building
    glm::mat4 inv_projection = glm::inverse(m_camera_data.projection);

    // Build clusters
    m_cluster_grid.build_clusters(m_camera_data.view, m_camera_data.projection, inv_projection,
                                  lights);
}

void
vulkan_render::rebuild_light_grid()
{
    KRG_check(m_light_grid.is_initialized(), "Light grid should be initialized");

    m_light_grid.clear();

    // Insert all universal lights into the grid (skip invalid/freed lights)
    for (uint32_t i = 0; i < m_cache.universal_lights.get_size(); ++i)
    {
        auto* light = m_cache.universal_lights.at(i);
        if (!light->is_valid())
            continue;
        m_light_grid.insert_light(light->slot(), light->gpu_data.position, light->gpu_data.radius);
    }
}

void
vulkan_render::upload_cluster_data(render::frame_state& frame)
{
    ZoneScopedN("Render::UploadClusters");

    if (!m_cluster_grid.is_initialized())
        return;

    const auto& config = m_cluster_grid.get_config();

    // std140 padded struct for cluster data (16 bytes per element)
    struct alignas(16) cluster_data_std140
    {
        uint32_t value;
    };

    // Upload cluster light counts (padded to std140)
    {
        const auto& counts = m_cluster_grid.get_cluster_light_counts();
        const size_t size = counts.size() * sizeof(cluster_data_std140);

        frame.buffers.cluster_counts.begin();
        auto* data = reinterpret_cast<cluster_data_std140*>(
            frame.buffers.cluster_counts.allocate_data((uint32_t)size));

        for (size_t i = 0; i < counts.size(); ++i)
        {
            data[i].value = counts[i];
        }
        frame.buffers.cluster_counts.end();
    }

    // Upload cluster light indices (padded to std140)
    {
        const auto& indices = m_cluster_grid.get_cluster_light_indices();
        const size_t size = indices.size() * sizeof(cluster_data_std140);

        frame.buffers.cluster_indices.begin();
        auto* data = reinterpret_cast<cluster_data_std140*>(
            frame.buffers.cluster_indices.allocate_data((uint32_t)size));

        for (size_t i = 0; i < indices.size(); ++i)
        {
            data[i].value = indices[i];
        }
        frame.buffers.cluster_indices.end();
    }

    // Upload cluster config
    {
        // Update config from current camera/grid state
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

        frame.buffers.cluster_config.begin();
        auto* data = frame.buffers.cluster_config.allocate_data(sizeof(gpu::cluster_grid_data));
        memcpy(data, &m_cluster_config, sizeof(gpu::cluster_grid_data));
        frame.buffers.cluster_config.end();
    }
}

void
vulkan_render::init_cluster_cull_compute()
{
    ZoneScopedN("Render::InitClusterCullCompute");

    auto path = glob::glob_state().get_resource_locator()->resource_dir(category::shaders_includes);
    auto shader_path = path / "cluster_cull.comp";

    kryga::utils::buffer shader_buffer;
    if (!kryga::utils::buffer::load(shader_path, shader_buffer))
    {
        ALOG_WARN("Failed to load cluster_cull.comp - GPU cluster culling disabled");
        m_use_gpu_cluster_cull = false;
        return;
    }

    m_cluster_cull_shader = std::make_unique<compute_shader_data>(AID("cluster_cull"));

    auto rc =
        vulkan_compute_shader_loader::create_compute_shader(*m_cluster_cull_shader, shader_buffer);
    if (rc != result_code::ok)
    {
        ALOG_WARN("Failed to create cluster cull compute shader - GPU cluster culling disabled");
        m_cluster_cull_shader.reset();
        m_use_gpu_cluster_cull = false;
        return;
    }

    ALOG_INFO("GPU cluster culling compute shader initialized");
}

void
vulkan_render::build_cluster_cull_descriptor_set(render::frame_state& frame)
{
    // Descriptor bindings match cluster_cull.comp:
    // set=0, binding=0: ClusterConfig (uniform)
    // set=0, binding=1: CameraData (uniform)
    // set=0, binding=2: LightBuffer (storage, readonly)
    // set=0, binding=3: LightCount (uniform)
    // set=0, binding=4: ClusterLightCounts (storage, writeonly)
    // set=0, binding=5: ClusterLightIndices (storage, writeonly)

    VkDescriptorBufferInfo config_info{.buffer = frame.buffers.cluster_config.buffer(),
                                       .offset = 0,
                                       .range = sizeof(gpu::cluster_grid_data)};

    VkDescriptorBufferInfo camera_info{.buffer = frame.buffers.dynamic_data.buffer(),
                                       .offset = 0,
                                       .range = sizeof(gpu::camera_data)};

    VkDescriptorBufferInfo lights_info{.buffer = frame.buffers.universal_lights.buffer(),
                                       .offset = 0,
                                       .range = frame.buffers.universal_lights.get_alloc_size()};

    // Light count is stored in a small uniform buffer
    // For simplicity, we'll use the cluster config buffer offset
    // Actually we need to pass light count separately - let's use push constants instead
    // For now, use the lights buffer size to infer count

    VkDescriptorBufferInfo counts_info{.buffer = frame.buffers.cluster_counts.buffer(),
                                       .offset = 0,
                                       .range = frame.buffers.cluster_counts.get_alloc_size()};

    VkDescriptorBufferInfo indices_info{.buffer = frame.buffers.cluster_indices.buffer(),
                                        .offset = 0,
                                        .range = frame.buffers.cluster_indices.get_alloc_size()};

    vk_utils::descriptor_builder::begin(glob::render_device::getr().descriptor_layout_cache(),
                                        frame.frame->m_dynamic_descriptor_allocator.get())
        .bind_buffer(0, &config_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                     VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_buffer(1, &camera_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                     VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_buffer(2, &lights_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_buffer(3, &counts_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     VK_SHADER_STAGE_COMPUTE_BIT)
        .bind_buffer(4, &indices_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     VK_SHADER_STAGE_COMPUTE_BIT)
        .build(m_cluster_cull_descriptor_set);
}

void
vulkan_render::setup_render_graph()
{
    // Register resources used by render graph
    // Names must match shader binding names (dyn_ prefix used in reflection)

    // Dynamic data buffer (UBO) - set 0
    m_render_graph.register_buffer(AID("dyn_camera_data"), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    // SSBOs - set 1
    m_render_graph.register_buffer(AID("dyn_object_buffer"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_gpu_universal_light_data"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_directional_lights_buffer"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // Cluster lighting SSBOs - set 1
    m_render_graph.register_buffer(AID("dyn_cluster_light_counts"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_cluster_light_indices"),
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_render_graph.register_buffer(AID("dyn_cluster_config"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // Materials SSBO - set 3
    m_render_graph.register_buffer(AID("dyn_material_buffer"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // Imported resources (externally managed)
    m_render_graph.import_resource(AID("swapchain"), rg_resource_type::image);
    m_render_graph.import_resource(AID("ui_target"), rg_resource_type::image);
    m_render_graph.import_resource(AID("picking_target"), rg_resource_type::image);

    // Register passes with dependencies
    // Compute pass: cluster culling (writes cluster buffers, reads lights)
    m_render_graph.add_compute_pass(
        AID("cluster_cull"),
        {m_render_graph.write(AID("dyn_cluster_light_counts")),
         m_render_graph.write(AID("dyn_cluster_light_indices")),
         m_render_graph.read(AID("dyn_gpu_universal_light_data"))},
        [this](VkCommandBuffer cmd)
        {
            if (m_use_clustered_lighting && m_use_gpu_cluster_cull && m_cluster_cull_shader)
            {
                dispatch_cluster_cull_impl(cmd);
            }
        });

    // UI pass: writes to UI render target
    // Graph handles render_pass->begin/end automatically
    m_render_graph.add_graphics_pass(AID("ui"), {m_render_graph.write(AID("ui_target"))},
                                     get_render_pass(AID("ui")), VkClearColorValue{0, 0, 0, 0},
                                     [this](VkCommandBuffer) { draw_ui(*m_current_frame); });

    // Picking pass: writes to picking target, reads shader resources
    m_render_graph.add_graphics_pass(
        AID("picking"),
        {m_render_graph.write(AID("picking_target")), m_render_graph.read(AID("dyn_camera_data")),
         m_render_graph.read(AID("dyn_object_buffer")),
         m_render_graph.read(AID("dyn_cluster_light_counts")),
         m_render_graph.read(AID("dyn_cluster_light_indices")),
         m_render_graph.read(AID("dyn_cluster_config"))},
        get_render_pass(AID("picking")), VkClearColorValue{0, 0, 0, 0},
        [this](VkCommandBuffer cmd)
        {
            for (auto& r : m_default_render_object_queue)
            {
                pipeline_ctx pctx{};
                if (!r.second.empty())
                {
                    bind_material(cmd, m_pick_mat, *m_current_frame, pctx, false);
                }
                draw_same_pipeline_objects_queue(cmd, pctx, r.second);
            }

            for (auto& r : m_outline_render_object_queue)
            {
                pipeline_ctx pctx{};
                if (!r.second.empty())
                {
                    bind_material(cmd, m_pick_mat, *m_current_frame, pctx, false);
                }
                draw_same_pipeline_objects_queue(cmd, pctx, r.second);
            }
        });

    // Main pass: writes to swapchain, reads all shader resources
    m_render_graph.add_graphics_pass(
        AID("main"),
        {m_render_graph.write(AID("swapchain")), m_render_graph.read(AID("ui_target")),
         // Shader resources
         m_render_graph.read(AID("dyn_camera_data")), m_render_graph.read(AID("dyn_object_buffer")),
         m_render_graph.read(AID("dyn_gpu_universal_light_data")),
         m_render_graph.read(AID("dyn_directional_lights_buffer")),
         m_render_graph.read(AID("dyn_cluster_light_counts")),
         m_render_graph.read(AID("dyn_cluster_light_indices")),
         m_render_graph.read(AID("dyn_cluster_config")),
         m_render_graph.read(AID("dyn_material_buffer"))},
        get_render_pass(AID("main")), VkClearColorValue{0, 0, 0, 1.0},
        [this](VkCommandBuffer) { draw_objects(*m_current_frame); });

    // Compile the graph (calculates execution order)
    bool result = m_render_graph.compile();

    KRG_check(result, "Pass should be always compiled");
}

void
vulkan_render::dispatch_cluster_cull_impl(VkCommandBuffer cmd)
{
    ZoneScopedN("Render::DispatchClusterCullImpl");

    // Bind compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_cluster_cull_shader->m_pipeline);

    // Bind descriptor set
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_cluster_cull_shader->m_pipeline_layout, 0, 1,
                            &m_cluster_cull_descriptor_set, 0, nullptr);

    // Push light count
    uint32_t num_lights = m_cache.universal_lights.get_size();
    vkCmdPushConstants(cmd, m_cluster_cull_shader->m_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(uint32_t), &num_lights);

    // Calculate dispatch size
    const auto& config = m_cluster_grid.get_config();
    uint32_t total_clusters = config.tiles_x * config.tiles_y * config.depth_slices;
    uint32_t workgroup_size = 64;  // Must match local_size_x in shader
    uint32_t num_workgroups = (total_clusters + workgroup_size - 1) / workgroup_size;

    // Dispatch compute shader
    vkCmdDispatch(cmd, num_workgroups, 1, 1);

    // Note: Barrier is handled by render graph, not here
}

}  // namespace render
}  // namespace kryga