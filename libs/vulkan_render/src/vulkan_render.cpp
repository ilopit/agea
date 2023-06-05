#include "vulkan_render/vulkan_render.h"

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render.h"
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
    auto device = glob::render_device::get();
    m_frames.resize(device->frame_size());

    for (size_t i = 0; i < m_frames.size(); ++i)
    {
        m_frames[i].frame = &device->frame(i);

        m_frames[i].m_object_buffer = device->create_buffer(
            OBJECTS_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].m_materials_buffer =
            device->create_buffer(INITIAL_MATERIAL_RANGE_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                  VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].m_lights_buffer =
            device->create_buffer(INITIAL_LIGHTS_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                  VMA_MEMORY_USAGE_CPU_TO_GPU);

        m_frames[i].m_dynamic_data_buffer = device->create_buffer(
            DYNAMIC_BUFFER_SIZE, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    }

    prepare_system_resources();
    prepare_ui_resources();
    prepare_ui_pipeline();
}

void
vulkan_render::deinit()
{
    m_frames.clear();
}

void
vulkan_render::set_camera(render::gpu_camera_data c)
{
    m_camera_data = c;
}

void
vulkan_render::draw_objects()
{
    auto device = glob::render_device::get();

    auto r = SDL_GetWindowFlags(glob::native_window::getr().handle());

    // TODO, rework
    if ((SDL_WINDOW_MINIMIZED & r) == SDL_WINDOW_MINIMIZED)
    {
        int i = 2;
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

    // make a clear-color from frame number. This will flash with a 120 frame period.
    VkClearValue clear_value{};
    clear_value.color = {{0.0f, 0.0f, 0.0, 1.0f}};

    // clear depth at 1
    VkClearValue depth_clear{};
    depth_clear.depthStencil.depth = 1.f;

    // start the main renderpass.
    // We will use the clear color from above, and the framebuffer of the index the swapchain gave
    // us

    auto width = (uint32_t)glob::native_window::get()->get_size().w;
    auto height = (uint32_t)glob::native_window::get()->get_size().h;

    if (width == 0 || height == 0)
    {
        int i = 2;
    }

    auto rp_info =
        vk_utils::make_renderpass_begin_info(device->render_pass(), VkExtent2D{width, height},
                                             device->framebuffers(swapchain_image_index));

    // connect clear values
    rp_info.clearValueCount = 2;
    VkClearValue clear_values[] = {clear_value, depth_clear};
    rp_info.pClearValues = &clear_values[0];

    vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

    draw_objects(current_frame);

    update_ui(current_frame);
    draw_ui(current_frame);

    // finalize the render pass
    vkCmdEndRenderPass(cmd);
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
    dyn.upload_data(m_scene_parameters);
    dyn.end();

    build_global_set(current_frame);
    build_light_set(current_frame);

    for (auto& r : m_default_render_object_queue)
    {
        draw_objects_queue(r.second, cmd, current_frame.m_object_buffer, dyn, current_frame);
    }

    if (!m_transparent_render_object_queue.empty())
    {
        update_transparent_objects_queue();
        draw_objects_queue(m_transparent_render_object_queue, cmd, current_frame.m_object_buffer,
                           dyn, current_frame);
    }
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
        .bind_buffer(1, &dynamic_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                     VK_SHADER_STAGE_FRAGMENT_BIT)
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
        (render::gpu_object_data*)frame.m_object_buffer.allocate_data(total_size);

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
vulkan_render::draw_objects_queue(render_line_conteiner& r,
                                  VkCommandBuffer cmd,
                                  vk_utils::vulkan_buffer& ssbo_buffer,
                                  vk_utils::vulkan_buffer& dyn_buffer,
                                  render::frame_state& current_frame)

{
    const uint32_t dummy_offest[] = {0, 0, 0, 0};

    uint32_t cur_material_idx = INVALID_GPU_INDEX;
    mesh_data* cur_mesh = nullptr;
    material_data* cur_material = nullptr;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    collect_lights();

    for (auto& obj : r)
    {
        if (cur_material_idx != obj->material->gpu_type_idx())
        {
            cur_material = obj->material;
            cur_material_idx = obj->material->gpu_type_idx();
            AGEA_check(cur_material, "Shouldn't be null");

            pipeline = cur_material->effect->m_pipeline;
            pipeline_layout = cur_material->effect->m_pipeline_layout;

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            auto& sm = m_materials_layout.at(cur_material_idx);

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

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                                    OBJECTS_descriptor_sets, 1, &m_objects_set, 4, dummy_offest);

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                                    MATERIALS_descriptor_sets, 1, &mat_data_set, 1, dummy_offest);

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                                    GLOBAL_descriptor_sets, 1, &m_global_set,
                                    dyn_buffer.get_dyn_offsets_count(),
                                    dyn_buffer.get_dyn_offsets_ptr());
        }

        // TODO, optimize
        if (obj->material->m_set)
        {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                                    TEXTURES_descriptor_sets, 1, &obj->material->m_set, 0, nullptr);
        }

        if (cur_mesh != obj->mesh)
        {
            cur_mesh = obj->mesh;
            AGEA_check(cur_mesh, "Should be null");

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &cur_mesh->m_vertex_buffer.buffer(), &offset);

            if (cur_mesh->has_indices())
            {
                vkCmdBindIndexBuffer(cmd, cur_mesh->m_index_buffer.buffer(), 0,
                                     VK_INDEX_TYPE_UINT32);
            }
        }

        m_obj_config.material_id = obj->material->gpu_idx();

        constexpr auto range = sizeof(render::gpu_push_constants);
        vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, range,
                           &m_obj_config);

        // we can now draw
        if (!cur_mesh->has_indices())
        {
            vkCmdDraw(cmd, cur_mesh->vertices_size(), 1, 0, obj->gpu_index());
        }
        else
        {
            vkCmdDrawIndexed(cmd, cur_mesh->indices_size(), 1, 0, 0, obj->gpu_index());
        }
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

    obj_data->set_index(id);

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
    auto& mat_id = mat_data->type_id();

    auto segment = m_materials_layout.find(mat_id);

    if (!segment)
    {
        segment = m_materials_layout.add(mat_id, mat_data->gpu_data.size(),
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
    auto& mat_id = mat_data->type_id();
    auto segment = m_materials_layout.find(mat_id);

    if (segment)
    {
        segment->release_id(mat_data->gpu_idx());
        mat_data->invalidate_ids();
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
        auto dst_ptr = ssbo_data + m->gpu_idx() * m->gpu_data.size();
        memcpy(dst_ptr, m->gpu_data.data(), m->gpu_data.size());
    }

    mats_to_update.clear();
}

frame_state&
vulkan_render::get_current_frame_transfer_data()
{
    return m_frames[glob::render_device::getr().get_current_frame_index()];
}

void
vulkan_render::prepare_system_resources()
{
    glob::vulkan_render_loader::getr().create_sampler(AID("default"),
                                                      VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);

    glob::vulkan_render_loader::getr().create_sampler(AID("font"),
                                                      VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);
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
}

void
vulkan_render::prepare_ui_pipeline()
{
    agea::utils::buffer vert, frag;

    auto path = glob::resource_locator::get()->resource(category::packages,
                                                        "base.apkg/class/shader_effects/ui");

    auto vert_path = path / "se_uioverlay.vert";
    agea::utils::buffer::load(vert_path, vert);

    auto frag_path = path / "se_uioverlay.frag";
    agea::utils::buffer::load(frag_path, frag);

    utils::dynamic_object_layout_sequence_builder<gpu_type> builder;
    builder.add_field(AID("pos"), agea::render::gpu_type::g_vec2, 1);
    builder.add_field(AID("uv"), agea::render::gpu_type::g_vec2, 1);
    builder.add_field(AID("color"), agea::render::gpu_type::g_color, 1);

    auto dol = builder.get_layout();

    auto vertex_input_description = render::convert_to_vertex_input_description(*dol);

    shader_effect_create_info se_ci;
    se_ci.vert_buffer = &vert;
    se_ci.frag_buffer = &frag;
    se_ci.render_pass = glob::render_device::getr().render_pass();
    se_ci.vert_input_description = &vertex_input_description;
    se_ci.is_wire = false;
    se_ci.enable_dynamic_state = true;
    se_ci.enable_alpha = true;
    se_ci.depth_compare_op = VK_COMPARE_OP_ALWAYS;

    m_ui_se = glob::vulkan_render_loader::getr().create_shader_effect(AID("se_ui"), se_ci);

    auto device = glob::render_device::get();

    std::vector<texture_sampler_data> samples(1);
    samples.front().texture = m_ui_txt;
    samples.front().slot = 0;

    m_ui_mat = glob::vulkan_render_loader::getr().create_material(
        AID("mat_ui"), AID("ui"), samples, *m_ui_se, utils::dynamic_object{});
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

        fs.m_ui_vertex_buffer = vk_utils::vulkan_buffer::create(
            device->get_vma_allocator_provider(), staging_buffer_ci, vma_ci);

        staging_buffer_ci.size = index_buffer_size;
        staging_buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        fs.m_ui_index_buffer = vk_utils::vulkan_buffer::create(device->get_vma_allocator_provider(),
                                                               staging_buffer_ci, vma_ci);
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

    // for (auto& ts : m_ui_mat->texture_samples)
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ui_se->m_pipeline_layout, 0,
                                1, &m_ui_mat->m_set, 0, nullptr);
    }

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

}  // namespace render
}  // namespace agea