#include "vulkan_render/agea_render.h"

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

#include <SDL.h>
#include <SDL_vulkan.h>
#include <SDL_events.h>

#include <native/native_window.h>

#include <utils/agea_log.h>
#include <utils/process.h>
#include <utils/clock.h>
#include <utils/dynamic_object.h>
#include <utils/dynamic_object_builder.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_sdl.h>

namespace agea
{
glob::vulkan_render::type glob::vulkan_render::type::s_instance;

namespace render
{
namespace
{
const uint32_t GLOBAL_descriptor_sets = 0;
const uint32_t OBJECTS_descriptor_sets = 1;
const uint32_t TEXTURES_descriptor_sets = 2;
const uint32_t MATERIALS_descriptor_sets = 3;

const uint32_t INITIAL_OBJECTS_RANGE_SIZE = 4 * 1024;
const uint32_t INITIAL_MATERIAL_SEGMENT_RANGE_SIZE = 1024;
const uint32_t INITIAL_MATERIAL_RANGE_SIZE = 10 * INITIAL_MATERIAL_SEGMENT_RANGE_SIZE;

const uint32_t INITIAL_LIGHTS_SEGMENT_SIZE = 1 * 1024;
const uint32_t INITIAL_LIGHTS_BUFFER_SIZE = 3 * 1024;

const uint32_t OBJECTS_BUFFER_SIZE = 16 * 1024;
const uint32_t LIGHTS_BUFFER_SIZE = 3 * 1024;
const uint32_t DYNAMIC_BUFFER_SIZE = 4 * 1024;
}  // namespace

vulkan_render::vulkan_render()
{
}

vulkan_render::~vulkan_render()
{
}

void
vulkan_render::init()
{
    auto& device = glob::render_device::getr();

    m_frames.resize(device.frame_size());

    for (size_t i = 0; i < m_frames.size(); ++i)
    {
        m_frames[i].frame = &device.frame(i);

        m_frames[i].m_object_buffer = device.create_buffer(
            OBJECTS_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].m_materials_buffer =
            device.create_buffer(INITIAL_MATERIAL_RANGE_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].m_lights_buffer =
            device.create_buffer(INITIAL_LIGHTS_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].m_dynamic_data_buffer = device.create_buffer(
            DYNAMIC_BUFFER_SIZE, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    prepare_render_passes();
    prepare_system_resources();
    prepare_ui_resources();
    prepare_ui_pipeline();
}

void
vulkan_render::deinit()
{
    m_render_passes.clear();

    m_frames.clear();
}

void
vulkan_render::set_camera(render::gpu_camera_data c)
{
    m_camera_data = c;
}

void
vulkan_render::draw_main()
{
    auto device = glob::render_device::get();

    auto r = SDL_GetWindowFlags(glob::native_window::getr().handle());

    // TODO, rework
    if ((SDL_WINDOW_MINIMIZED & r) == SDL_WINDOW_MINIMIZED)
    {
        return;
    }

    device->switch_frame_indeces();

    auto& current_frame = m_frames[device->get_current_frame_index()];

    // wait until the gpu has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(device->vk_device(), 1, &current_frame.frame->m_render_fence, true,
                             1000000000));
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

    auto rp = m_render_passes[AID("ui")];
    rp->begin(cmd, swapchain_image_index, width, height, VkClearColorValue{0, 0, 0, 0});
    draw_ui(current_frame);
    rp->end(cmd);

    rp = m_render_passes[AID("picking")];
    rp->begin(cmd, swapchain_image_index, width, height, VkClearColorValue{0, 0, 0, 0});

    for (auto& r : m_default_render_object_queue)
    {
        pipeline_ctx pctx{};

        if (!r.second.empty())
        {
            bind_material(cmd, m_pick_mat, current_frame, pctx, false);
        }

        draw_same_pipeline_objects_queue(cmd, pctx, r.second);
    }

    rp->end(cmd);

    rp = m_render_passes[AID("main")];
    rp->begin(cmd, swapchain_image_index, width, height, VkClearColorValue{0, 0, 0, 1.0});
    draw_objects(current_frame);
    rp->end(cmd);

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

    // TRANSPATENT
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
    if (current_frame.has_obj_data())
    {
        upload_obj_data(current_frame);
        current_frame.reset_obj_data();
    }

    if (current_frame.has_mat_data())
    {
        upload_material_data(current_frame);
        current_frame.reset_mat_data();
    }

    if (current_frame.has_light_data())
    {
        upload_light_data(current_frame);
        current_frame.reset_light_data();
    }

    auto& dyn = current_frame.m_dynamic_data_buffer;

    dyn.begin();
    dyn.upload_data(m_camera_data);
    dyn.end();

    build_global_set(current_frame);
    build_light_set(current_frame);

    collect_lights();
}

void
vulkan_render::build_global_set(render::frame_state& current_frame)
{
    VkDescriptorBufferInfo dynamic_info{};
    dynamic_info.buffer = current_frame.m_dynamic_data_buffer.buffer();
    dynamic_info.offset = 0;
    dynamic_info.range = current_frame.m_dynamic_data_buffer.get_offset();

    vk_utils::descriptor_builder::begin(glob::render_device::getr().descriptor_layout_cache(),
                                        current_frame.frame->m_dynamic_descriptor_allocator.get())
        .bind_buffer(0, &dynamic_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(m_global_set);
}

void
vulkan_render::build_light_set(render::frame_state& current_frame)
{
    VkDescriptorBufferInfo directional_light_info{};
    directional_light_info.buffer = current_frame.m_lights_buffer.buffer();
    directional_light_info.offset = 0;
    directional_light_info.range = INITIAL_LIGHTS_SEGMENT_SIZE;

    VkDescriptorBufferInfo point_light_info{};
    point_light_info.buffer = current_frame.m_lights_buffer.buffer();
    point_light_info.offset = INITIAL_LIGHTS_SEGMENT_SIZE;
    point_light_info.range = INITIAL_LIGHTS_SEGMENT_SIZE;

    VkDescriptorBufferInfo spot_light_info{};
    spot_light_info.buffer = current_frame.m_lights_buffer.buffer();
    spot_light_info.offset = INITIAL_LIGHTS_SEGMENT_SIZE * 2;
    spot_light_info.range = INITIAL_LIGHTS_SEGMENT_SIZE;

    VkDescriptorBufferInfo object_buffer_info{};
    object_buffer_info.buffer = current_frame.m_object_buffer.buffer();
    object_buffer_info.offset = 0;
    object_buffer_info.range = current_frame.m_object_buffer.get_alloc_size();

    vk_utils::descriptor_builder::begin(glob::render_device::getr().descriptor_layout_cache(),
                                        current_frame.frame->m_dynamic_descriptor_allocator.get())
        .bind_buffer(0, &object_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                     VK_SHADER_STAGE_VERTEX_BIT)
        .bind_buffer(1, &directional_light_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                     VK_SHADER_STAGE_FRAGMENT_BIT)
        .bind_buffer(2, &point_light_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                     VK_SHADER_STAGE_FRAGMENT_BIT)
        .bind_buffer(3, &spot_light_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                     VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(m_objects_set);

    AGEA_check(m_objects_set, "Should never happens");
}

void
vulkan_render::upload_obj_data(render::frame_state& frame)
{
    const auto total_size = m_objects_id.get_ids_in_fly() * sizeof(gpu_object_data);

    bool reallocated = false;
    if (total_size >= frame.m_object_buffer.get_alloc_size())
    {
        auto old_buffer_tb = std::move(frame.m_object_buffer);

        frame.m_object_buffer = glob::render_device::getr().create_buffer(
            total_size * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        ALOG_INFO("Reallocating obj buffer {0} => {1}", old_buffer_tb.get_alloc_size(),
                  frame.m_object_buffer.get_alloc_size());

        old_buffer_tb.begin();
        frame.m_object_buffer.begin();

        memcpy(frame.m_object_buffer.get_data(), old_buffer_tb.get_data(),
               old_buffer_tb.get_alloc_size());

        old_buffer_tb.end();

        reallocated = true;
    }

    if (!reallocated)
    {
        frame.m_object_buffer.begin();
    }

    auto gpu_object_data_begin =
        (render::gpu_object_data*)frame.m_object_buffer.allocate_data((uint32_t)total_size);

    upload_gpu_object_data(gpu_object_data_begin);

    frame.m_object_buffer.end();
}

void
vulkan_render::upload_light_data(render::frame_state& frame)
{
    auto& buffer_td = frame.m_lights_buffer;

    buffer_td.begin();

    auto direct =
        (render::gpu_directional_light_data*)buffer_td.allocate_data(INITIAL_LIGHTS_SEGMENT_SIZE);

    auto point_data =
        (render::gpu_point_light_data*)buffer_td.allocate_data(INITIAL_LIGHTS_SEGMENT_SIZE);

    auto spot_light =
        (render::gpu_spot_light_data*)buffer_td.allocate_data(INITIAL_LIGHTS_SEGMENT_SIZE);

    for (auto pd : frame.m_lights_queue)
    {
        switch (pd->m_type)
        {
        case light_type::directional_light_data:
            direct[pd->m_gpu_id] = pd->m_data.directional;
            break;

        case light_type::point_light_data:
            point_data[pd->m_gpu_id] = pd->m_data.point;
            break;
        case light_type::spot_light_data:
            spot_light[pd->m_gpu_id] = pd->m_data.spot;
            break;
        default:
            break;
        }
    }

    frame.m_lights_queue.clear();

    buffer_td.end();
}

void
vulkan_render::upload_material_data(render::frame_state& frame)
{
    auto total_size = m_materials_layout.calc_new_size();

    bool reallocated = false;

    vk_utils::vulkan_buffer old_buffer_tb;

    if (total_size >= frame.m_materials_buffer.get_alloc_size())
    {
        old_buffer_tb = std::move(frame.m_materials_buffer);

        frame.m_materials_buffer = glob::render_device::getr().create_buffer(
            total_size * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        ALOG_INFO("Reallocating material buffer {0} => {1}", old_buffer_tb.get_alloc_size(),
                  frame.m_materials_buffer.get_alloc_size());

        reallocated = true;
    }

    if (reallocated)
    {
        old_buffer_tb.begin();
    }

    frame.m_materials_buffer.begin();

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
                memcpy(frame.m_materials_buffer.get_data() + dst_offset,
                       old_buffer_tb.get_data() + src_offset, size);
            }
            else
            {
                memmove(frame.m_materials_buffer.get_data() + dst_offset,
                        old_buffer_tb.get_data() + src_offset, size);
            }
        }
    }

    if (reallocated)
    {
        old_buffer_tb.end();
    }

    auto mat_begin = frame.m_materials_buffer.get_data();

    for (int i = 0; i < m_materials_layout.get_segments_size(); ++i)
    {
        auto& sm = m_materials_layout.at(i);
        auto& mat_set_queue = frame.m_materias_queue_set[sm.index];

        upload_gpu_materials_data(mat_begin + sm.offset, mat_set_queue);
    }

    frame.m_materials_buffer.end();
    m_materials_layout.reset_dirty_layout();
}

void
vulkan_render::draw_multi_pipeline_objects_queue(render_line_conteiner& r,
                                                 VkCommandBuffer cmd,
                                                 render::frame_state& current_frame)
{
    mesh_data* cur_mesh = nullptr;

    pipeline_ctx pctx{};

    for (auto& obj : r)
    {
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
                                        TEXTURES_descriptor_sets, 1,
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
vulkan_render::draw_objects_queue(render_line_conteiner& r,
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
                                                const render_line_conteiner& r,
                                                bool rebind_images)
{
    mesh_data* cur_mesh = nullptr;
    uint32_t cur_material_idx = pctx.cur_material_idx;

    for (auto& obj : r)
    {
        if (rebind_images && cur_material_idx != obj->material->gpu_idx())
        {
            cur_material_idx = obj->material->gpu_idx();

            if (obj->material->has_textures())
            {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pctx.pipeline_layout,
                                        TEXTURES_descriptor_sets, 1,
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
                           const render::object_data* obj)
{
    auto cur_mesh = obj->mesh;
    m_obj_config.material_id = obj->material->gpu_idx();

    constexpr auto range = sizeof(render::gpu_push_constants);
    vkCmdPushConstants(cmd, pctx.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, range,
                       &m_obj_config);

    // we can now draw
    if (!obj->mesh->has_indices())
    {
        vkCmdDraw(cmd, cur_mesh->vertices_size(), 1, 0, obj->gpu_index());
    }
    else
    {
        vkCmdDrawIndexed(cmd, cur_mesh->indices_size(), 1, 0, 0, obj->gpu_index());
    }
}

void
vulkan_render::bind_mesh(VkCommandBuffer cmd, mesh_data* cur_mesh)
{
    AGEA_check(cur_mesh, "Should be null");

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

    const uint32_t dummy_offest[] = {0, 0, 0, 0};

    if (object)
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline_layout,
                                OBJECTS_descriptor_sets, 1, &m_objects_set, 4, dummy_offest);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline_layout,
                                GLOBAL_descriptor_sets, 1, &m_global_set,
                                current_frame.m_dynamic_data_buffer.get_dyn_offsets_count(),
                                current_frame.m_dynamic_data_buffer.get_dyn_offsets_ptr());
    }

    if (cur_material->has_gpu_data())
    {
        auto& sm = m_materials_layout.at(cur_material->gpu_idx());
        VkDescriptorBufferInfo mat_buffer_info{};
        mat_buffer_info.buffer = current_frame.m_materials_buffer.buffer();
        mat_buffer_info.offset = sm.offset;
        mat_buffer_info.range = sm.get_allocated_size();

        VkDescriptorSet mat_data_set{};
        vk_utils::descriptor_builder::begin(
            glob::render_device::getr().descriptor_layout_cache(),
            current_frame.frame->m_dynamic_descriptor_allocator.get())
            .bind_buffer(0, &mat_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                         VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(mat_data_set);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline_layout,
                                MATERIALS_descriptor_sets, 1, &mat_data_set, 1, dummy_offest);
    }

    if (cur_material->has_textures())
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.pipeline_layout,
                                TEXTURES_descriptor_sets, 1, &cur_material->get_textures_ds(), 0,
                                nullptr);
    }
}

void
vulkan_render::push_config(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout, uint32_t mat_id)
{
    m_obj_config.directional_light_id = 0U;
    m_obj_config.point_lights_size = 0U;
    m_obj_config.spot_lights_size = 0U;

    auto& lts = glob::vulkan_render_loader::getr().get_lights();
}

void
vulkan_render::add_object(render::object_data* obj_data)
{
    AGEA_check(obj_data, "Should be always valid");

    auto id = m_objects_id.alloc_id();

    obj_data->set_index((uint32_t)id);

    if (obj_data->outlined)
    {
        AGEA_check(obj_data->queue_id != "transparent", "Not supported!");

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
vulkan_render::drop_object(render::object_data* obj_data)
{
    AGEA_check(obj_data, "Should be always valid");

    m_objects_id.release_id(obj_data->gpu_index());

    const std::string id = obj_data->queue_id;

    auto& bucket = m_default_render_object_queue[id];

    auto itr = bucket.find(obj_data);

    AGEA_check(itr != bucket.end(), "Dropping from missing bucket");

    bucket.swap_and_remove(itr);

    if (bucket.get_size() == 0)
    {
        ALOG_TRACE("Dropping old queue");
        m_default_render_object_queue.erase(id);
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
            while (segment->index >= q.m_materias_queue_set.get_size())
            {
                q.m_materias_queue_set.emplace_back();
            }
        }
    }
    mat_data->set_idexes(segment->alloc_id(), segment->index);
}

void
vulkan_render::drop_material(render::material_data* mat_data)
{
    auto& mat_id = mat_data->get_type_id();
    auto segment = m_materials_layout.find(mat_id);

    if (segment)
    {
        segment->release_id(mat_data->gpu_idx());
        mat_data->invalidate_gpu_idexes();
    }
}

void
vulkan_render::schedule_material_data_gpu_upload(render::material_data* md)
{
    for (auto& q : m_frames)
    {
        q.m_materias_queue_set[md->gpu_type_idx()].emplace_back(md);
        q.has_materials = true;
    }
}

void
vulkan_render::schedule_game_data_gpu_upload(render::object_data* obj_date)
{
    for (auto& q : m_frames)
    {
        q.m_objects_queue.emplace_back(obj_date);
    }
}

void
vulkan_render::schedule_light_data_gpu_upload(render::light_data* ld)
{
    for (auto& q : m_frames)
    {
        q.m_lights_queue.emplace_back(ld);
    }
}

void
vulkan_render::clear_upload_queue()
{
    for (auto& q : m_frames)
    {
        q.clear_upload_queues();
    }
}

void
vulkan_render::collect_lights()
{
    auto& lights = glob::vulkan_render_loader::getr().get_lights();

    m_obj_config.point_lights_size = 0U;
    m_obj_config.spot_lights_size = 0U;

    for (auto& l : lights)
    {
        switch (l.second->m_type)
        {
        case light_type::directional_light_data:
            m_obj_config.directional_light_id = l.second->m_gpu_id;
            break;
        case light_type::point_light_data:
            m_obj_config.point_light_ids[m_obj_config.point_lights_size] = l.second->m_gpu_id;
            ++m_obj_config.point_lights_size;
            break;
        case light_type::spot_light_data:
            m_obj_config.spot_light_ids[m_obj_config.spot_lights_size] = l.second->m_gpu_id;
            ++m_obj_config.spot_lights_size;

            break;
        default:
            break;
        }
    }
}

void
vulkan_render::upload_gpu_object_data(render::gpu_object_data* object_SSBO)
{
    auto& to_update = get_current_frame_transfer_data().m_objects_queue;

    if (to_update.empty())
    {
        return;
    }

    for (auto obj : to_update)
    {
        object_SSBO[obj->gpu_index()] = obj->gpu_data;
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

    auto width = (uint32_t)glob::native_window::get()->get_size().w;
    auto height = (uint32_t)glob::native_window::get()->get_size().h;

    {
        auto main_pass =
            render_pass_builder()
                .set_color_format(device.get_swapchain_format())
                .set_depth_format(VK_FORMAT_D32_SFLOAT_S8_UINT)
                .set_width_depth(width, height)
                .set_color_images(device.get_swapchain_image_views(), device.get_swapchain_images())
                .set_preset(render_pass_builder::presets::swapchain)
                .build();

        m_render_passes[AID("main")] = std::move(main_pass);
    }

    VkExtent3D image_extent = {width, height, 1};

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
                .set_width_depth(width, height)
                .set_color_images(std::vector<vk_utils::vulkan_image_view_sptr>{image_view},
                                  std::vector<vk_utils::vulkan_image_sptr>{image})
                .set_enable_stencil(false)
                .set_preset(render_pass_builder::presets::buffer)
                .build();

        m_render_passes[AID("ui")] = std::move(ui_pass);
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
                .set_width_depth(width, height)
                .set_color_images(std::vector<vk_utils::vulkan_image_view_sptr>{image_view},
                                  std::vector<vk_utils::vulkan_image_sptr>{image})
                .set_preset(render_pass_builder::presets::picking)
                .build();

        m_render_passes[AID("picking")] = std::move(picking_pass);
    }
}

void
vulkan_render::prepare_system_resources()
{
    glob::vulkan_render_loader::getr().create_sampler(AID("default"),
                                                      VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);

    glob::vulkan_render_loader::getr().create_sampler(AID("font"),
                                                      VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);

    agea::utils::buffer vert, frag;

    auto path = glob::resource_locator::get()->resource(category::packages,
                                                        "base.apkg/class/shader_effects");

    auto vert_path = path / "error/se_error.vert";
    agea::utils::buffer::load(vert_path, vert);

    auto frag_path = path / "error/se_error.frag";
    agea::utils::buffer::load(frag_path, frag);

    shader_effect_create_info se_ci;
    se_ci.vert_buffer = &vert;
    se_ci.frag_buffer = &frag;
    se_ci.rp = m_render_passes[AID("main")].get();
    se_ci.is_wire = false;
    se_ci.enable_dynamic_state = false;
    se_ci.alpha = alpha_mode::none;
    se_ci.cull_mode = VK_CULL_MODE_NONE;

    shader_effect_data* sed = nullptr;
    auto rc = glob::vulkan_render_loader::getr().create_shader_effect(AID("se_error"), se_ci, sed);
    AGEA_check(rc == result_code::ok && sed, "Always should be good!");

    vert_path = path / "system/se_outline.vert";
    agea::utils::buffer::load(vert_path, vert);

    frag_path = path / "system/se_outline.frag";
    agea::utils::buffer::load(frag_path, frag);

    se_ci.ds_mode = depth_stencil_mode::outline;

    sed = nullptr;
    rc = glob::vulkan_render_loader::getr().create_shader_effect(AID("se_outline"), se_ci, sed);
    AGEA_check(rc == result_code::ok && sed, "Always should be good!");

    std::vector<texture_sampler_data> sd;
    m_outline_mat = glob::vulkan_render_loader::getr().create_material(
        AID("mat_outline"), AID("outline"), sd, *sed, utils::dynobj{});

    vert_path = path / "system/se_pick.vert";
    agea::utils::buffer::load(vert_path, vert);

    frag_path = path / "system/se_pick.frag";
    agea::utils::buffer::load(frag_path, frag);

    se_ci.ds_mode = depth_stencil_mode::none;
    se_ci.rp = m_render_passes[AID("picking")].get();
    sed = nullptr;

    rc = glob::vulkan_render_loader::getr().create_shader_effect(AID("se_pick"), se_ci, sed);
    AGEA_check(rc == result_code::ok && sed, "Always should be good!");

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
              [](render::object_data* l, render::object_data* r)
              { return l->distance_to_camera > r->distance_to_camera; });
}

void
vulkan_render::prepare_ui_resources()
{
    ImGuiIO& io = ImGui::GetIO();

    // Create font texture
    auto path = glob::resource_locator::get()->resource(category::fonts, "Roboto-Medium.ttf");
    auto f = path.str();

    io.Fonts->AddFontFromFileTTF(f.c_str(), 28.0f);

    int tex_width = 0, tex_height = 0;
    unsigned char* font_data = nullptr;
    io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

    auto size = tex_width * tex_height * 4 * sizeof(char);

    agea::utils::buffer image_raw_buffer;
    image_raw_buffer.resize(size);
    memcpy(image_raw_buffer.data(), font_data, size);

    m_ui_txt = glob::vulkan_render_loader::getr().create_texture(AID("font"), image_raw_buffer,
                                                                 tex_width, tex_height);

    m_ui_target_txt = glob::vulkan_render_loader::getr().create_texture(
        AID("ui_copy_txt"), m_render_passes[AID("ui")]->get_color_images()[0],
        m_render_passes[AID("ui")]->get_color_image_views()[0]);
}

void
vulkan_render::prepare_ui_pipeline()
{
    auto path = glob::resource_locator::get()->resource(category::packages,
                                                        "base.apkg/class/shader_effects/ui");

    {
        agea::utils::buffer vert, frag;

        auto vert_path = path / "se_uioverlay.vert";
        agea::utils::buffer::load(vert_path, vert);

        auto frag_path = path / "se_uioverlay.frag";
        agea::utils::buffer::load(frag_path, frag);

        auto layout = render::gpu_dynobj_builder()
                          .set_id(AID("interface"))
                          .add_field(AID("in_pos"), agea::render::gpu_type::g_vec2, 1)
                          .add_field(AID("in_UV"), agea::render::gpu_type::g_vec2, 1)
                          .add_field(AID("in_color"), agea::render::gpu_type::g_color, 1)
                          .finalize();

        shader_effect_create_info se_ci;
        se_ci.vert_buffer = &vert;
        se_ci.frag_buffer = &frag;
        se_ci.rp = m_render_passes[AID("ui")].get();
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = true;
        se_ci.alpha = alpha_mode::ui;
        se_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;
        se_ci.expected_input_vertex_layout = std::move(layout);

        glob::vulkan_render_loader::getr().create_shader_effect(AID("se_ui"), se_ci, m_ui_se);

        std::vector<texture_sampler_data> samples(1);
        samples.front().texture = m_ui_txt;
        samples.front().slot = 0;

        m_ui_mat = glob::vulkan_render_loader::getr().create_material(
            AID("mat_ui"), AID("ui"), samples, *m_ui_se, utils::dynobj{});
    }
    {
        agea::utils::buffer vert, frag;

        auto vert_path = path / "se_upload.vert";
        agea::utils::buffer::load(vert_path, vert);

        auto frag_path = path / "se_upload.frag";
        agea::utils::buffer::load(frag_path, frag);

        shader_effect_create_info se_ci;
        se_ci.vert_buffer = &vert;
        se_ci.frag_buffer = &frag;
        se_ci.rp = m_render_passes[AID("main")].get();
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = false;
        se_ci.alpha = alpha_mode::ui;
        se_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;

        glob::vulkan_render_loader::getr().create_shader_effect(AID("se_ui_copy"), se_ci,
                                                                m_ui_copy_se);

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
    if ((fs.m_ui_vertex_count != im_draw_data->TotalVtxCount) ||
        (fs.m_ui_index_count != im_draw_data->TotalIdxCount))
    {
        fs.m_ui_vertex_count = im_draw_data->TotalVtxCount;
        fs.m_ui_index_count = im_draw_data->TotalIdxCount;

        VkBufferCreateInfo staging_buffer_ci = {};
        staging_buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        staging_buffer_ci.pNext = nullptr;
        // this is the total size, in bytes, of the buffer we are allocating

        staging_buffer_ci.size = vertex_buffer_size;
        staging_buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        VmaAllocationCreateInfo vma_ci = {};
        vma_ci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        fs.m_ui_vertex_buffer = vk_utils::vulkan_buffer::create(staging_buffer_ci, vma_ci);

        staging_buffer_ci.size = index_buffer_size;
        staging_buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        fs.m_ui_index_buffer = vk_utils::vulkan_buffer::create(staging_buffer_ci, vma_ci);
    }

    // Upload data
    fs.m_ui_vertex_buffer.begin();
    fs.m_ui_index_buffer.begin();

    auto vtx_dst = (ImDrawVert*)fs.m_ui_vertex_buffer.allocate_data((uint32_t)vertex_buffer_size);
    auto idx_dst = (ImDrawIdx*)fs.m_ui_index_buffer.allocate_data((uint32_t)index_buffer_size);

    for (int n = 0; n < im_draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = im_draw_data->CmdLists[n];
        memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += cmd_list->VtxBuffer.Size;
        idx_dst += cmd_list->IdxBuffer.Size;
    }

    fs.m_ui_vertex_buffer.end();
    fs.m_ui_index_buffer.end();

    fs.m_ui_vertex_buffer.flush();
    fs.m_ui_index_buffer.flush();
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
    vkCmdBindVertexBuffers(cmd, 0, 1, &fs.m_ui_vertex_buffer.buffer(), offsets);
    vkCmdBindIndexBuffer(cmd, fs.m_ui_index_buffer.buffer(), 0, VK_INDEX_TYPE_UINT16);

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

uint32_t
vulkan_render::object_id_under_coordinate(uint32_t x, uint32_t y)
{
    auto width = (uint32_t)glob::native_window::get()->get_size().w;
    auto height = (uint32_t)glob::native_window::get()->get_size().h;

    // Source for the copy is the last rendered swapchain image
    auto src_image = m_render_passes[AID("picking")]->get_color_images()[0]->image();

    // Create the linear tiled destination image to copy to and to read the memory from

    auto extent = VkExtent3D{width, height, 1};

    VkImageCreateInfo imageCreateCI =
        vk_utils::make_image_create_info(VK_FORMAT_R8G8B8A8_UNORM, 0, extent);
    imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
    imageCreateCI.arrayLayers = 1;
    imageCreateCI.mipLevels = 1;
    imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateCI.tiling = VK_IMAGE_TILING_LINEAR;
    imageCreateCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // Create the image

    VmaAllocationCreateInfo vma_allocinfo = {};
    vma_allocinfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    vma_allocinfo.requiredFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    auto dst_image = vk_utils::vulkan_image::create(
        glob::render_device::getr().get_vma_allocator_provider(), imageCreateCI, vma_allocinfo);

    auto cmd_buf_ai = vk_utils::make_command_buffer_allocate_info(
        glob::render_device::getr().m_upload_context.m_command_pool, 1);

    VkCommandBuffer cmd_buffer;
    vkAllocateCommandBuffers(glob::render_device::getr().vk_device(), &cmd_buf_ai, &cmd_buffer);

    VkCommandBufferBeginInfo cmdBufInfo = vk_utils::make_command_buffer_begin_info();
    vkBeginCommandBuffer(cmd_buffer, &cmdBufInfo);

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

    // Get layout of the image (including row pitch)
    VkImageSubresource sub_resource{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
    VkSubresourceLayout sub_resource_layout;
    vkGetImageSubresourceLayout(glob::render_device::getr().vk_device(), dst_image.image(),
                                &sub_resource, &sub_resource_layout);

    // Map image memory so we can start copying from it

    auto mp = ImGui::GetIO().MousePos;

    auto data = dst_image.map();

    data += sub_resource_layout.offset + (x + y * width) * 4;

    uint32_t pixel = 0;

    memcpy(&pixel, data, 4);

    std::cout << std::format("{:x}", pixel) << std::endl;

    dst_image.unmap();

    return 1;
}

}  // namespace render
}  // namespace agea