#include "vulkan_render/vulkan_render.h"

#include "vulkan_render/render_device.h"
#include "vulkan_render/vulkan_render.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/vk_descriptors.h"

#include <vulkan_render_types/vulkan_initializers.h>
#include <vulkan_render_types/vulkan_texture_data.h>
#include <vulkan_render_types/vulkan_types.h>
#include <vulkan_render_types/vulkan_material_data.h>
#include <vulkan_render_types/vulkan_shader_data.h>
#include <vulkan_render_types/vulkan_mesh_data.h>
#include <vulkan_render_types/vulkan_shader_effect_data.h>
#include <vulkan_render_types/vulkan_render_data.h>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <SDL_events.h>

#include <native/native_window.h>
#include <utils/agea_log.h>
#include <utils/process.h>
#include <utils/clock.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_vulkan.h>

namespace agea
{
glob::vulkan_render::type glob::vulkan_render::type::s_instance;

namespace
{
const uint32_t GLOBAL_descriptor_sets = 0;
const uint32_t OBJECTS_descriptor_sets = 1;
const uint32_t TEXTURES_descriptor_sets = 2;
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
    m_transfer_queue.resize(device->frame_size());
}

void
vulkan_render::set_camera(render::gpu_camera_data c)
{
    m_camera_data = c;
}

void
vulkan_render::draw()
{
    auto device = glob::render_device::get();

    device->switch_frame_indeces();

    auto& current_frame = device->get_current_frame();

    // wait until the gpu has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(
        vkWaitForFences(device->vk_device(), 1, &current_frame.m_render_fence, true, 1000000000));
    VK_CHECK(vkResetFences(device->vk_device(), 1, &current_frame.m_render_fence));

    ImGui::Render();
    current_frame.m_dynamic_descriptor_allocator->reset_pools();

    // now that we are sure that the commands finished executing, we can safely reset the command
    // buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(current_frame.m_main_command_buffer, 0));

    // request image from the swapchain
    uint32_t swapchainImageIndex = 0U;
    VK_CHECK(vkAcquireNextImageKHR(device->vk_device(), device->swapchain(), 1000000000,
                                   current_frame.m_present_semaphore, nullptr,
                                   &swapchainImageIndex));

    // naming it cmd for shorter writing
    VkCommandBuffer cmd = current_frame.m_main_command_buffer;

    // begin the command buffer recording. We will use this command buffer exactly once, so we want
    // to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo =
        render::utils::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // make a clear-color from frame number. This will flash with a 120 frame period.
    VkClearValue clearValue;
    clearValue.color = {{0.0f, 0.0f, 0.0, 1.0f}};

    // clear depth at 1
    VkClearValue depthClear;
    depthClear.depthStencil.depth = 1.f;

    // start the main renderpass.
    // We will use the clear color from above, and the framebuffer of the index the swapchain gave
    // us

    auto width = (uint32_t)glob::native_window::get()->get_size().w;
    auto height = (uint32_t)glob::native_window::get()->get_size().h;

    VkRenderPassBeginInfo rpInfo =
        render::utils::renderpass_begin_info(device->render_pass(), VkExtent2D{width, height},
                                             device->framebuffers(swapchainImageIndex));

    // connect clear values
    rpInfo.clearValueCount = 2;
    VkClearValue clearValues[] = {clearValue, depthClear};
    rpInfo.pClearValues = &clearValues[0];
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    draw_new_objects(cmd, current_frame);

    //::agea::glob::ui::get()->draw(cmd);

    // finalize the render pass
    vkCmdEndRenderPass(cmd);
    // finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    // prepare the submission to the queue.
    // we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is
    // ready we will signal the _renderSemaphore, to signal that rendering has finished

    VkSubmitInfo submit = render::utils::submit_info(&cmd);
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    submit.pWaitDstStageMask = &waitStage;

    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &current_frame.m_present_semaphore;

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &current_frame.m_render_semaphore;

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit(device->vk_graphics_queue(), 1, &submit, current_frame.m_render_fence));

    // prepare present
    //  this will put the image we just rendered to into the visible window.
    //  we want to wait on the _renderSemaphore for that,
    //  as its necessary that drawing commands have finished before the image is displayed to the
    //  user
    VkPresentInfoKHR presentInfo = render::utils::present_info();

    presentInfo.pSwapchains = &device->swapchain();
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &current_frame.m_render_semaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(device->vk_graphics_queue(), &presentInfo));
}

void
vulkan_render::draw_new_objects(VkCommandBuffer cmd, render::frame_data& current_frame)
{
    auto device = glob::render_device::get();

    auto& object_tb = current_frame.m_object_buffer;

    if (get_current_frame_transfer_data().has_data())
    {
        object_tb.begin();
        auto gpu_object_data_begin = (render::gpu_object_data*)object_tb.allocate_data(
            sizeof(render::gpu_object_data) * 128);

        update_gpu_object_data(gpu_object_data_begin);

        for (auto& i : m_ssbo_range)
        {
            auto gpu_material_data_begin = object_tb.allocate_data(i.second);
            update_gpu_materials_data(
                gpu_material_data_begin,
                get_current_frame_transfer_data().m_materias_queue_set[i.first]);
        }
        object_tb.end();
    }

    VkDescriptorBufferInfo object_buffer_info{};
    object_buffer_info.buffer = current_frame.m_object_buffer.buffer();
    object_buffer_info.offset = 0;
    object_buffer_info.range = object_tb.get_offset();

    VkDescriptorSet object_data_set{};
    vk_utils::descriptor_builder::begin(device->descriptor_layout_cache(),
                                        current_frame.m_dynamic_descriptor_allocator.get())
        .bind_buffer(0, &object_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                     VK_SHADER_STAGE_VERTEX_BIT)
        .bind_buffer(1, &object_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                     VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(object_data_set);

    //     auto c = glob::level::getr()
    //                  .find_component(AID("point_light__light_component"))
    //                  ->as<model::game_object_component>();

    m_scene_parameters.lights_color = glm::vec4{1.f, 1.f, 1.f, 0.f};
    m_scene_parameters.lights_position = glm::vec4{1.f, 1.f, 1.f, 0.f};

    auto& dyn = current_frame.m_dynamic_data_buffer;

    dyn.begin();
    dyn.upload_data(m_camera_data);
    dyn.upload_data(m_scene_parameters);
    dyn.end();

    VkDescriptorBufferInfo dynamic_info{};
    dynamic_info.buffer = current_frame.m_dynamic_data_buffer.buffer();
    dynamic_info.offset = 0;
    dynamic_info.range = dyn.get_offset();

    VkDescriptorSet global_set;
    vk_utils::descriptor_builder::begin(device->descriptor_layout_cache(),
                                        current_frame.m_dynamic_descriptor_allocator.get())
        .bind_buffer(0, &dynamic_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .bind_buffer(1, &dynamic_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                     VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(global_set);

    for (auto& r : m_default_render_objec_queue)
    {
        draw_objects(r.second, cmd, object_tb, object_data_set, dyn, global_set);
    }

    if (m_transparent_render_object_queue.empty())
    {
        update_transparent_objects_queue();
        draw_objects(m_transparent_render_object_queue, cmd, object_tb, object_data_set, dyn,
                     global_set);
    }
}

void
vulkan_render::draw_objects(render_line_conteiner& r,
                            VkCommandBuffer cmd,
                            render::transit_buffer& obj_tb,
                            VkDescriptorSet obj_ds,
                            render::transit_buffer& dyn_tb,
                            VkDescriptorSet global_ds)

{
    render::mesh_data* cur_mesh = nullptr;
    render::material_data* cur_material = nullptr;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    uint32_t objects_to_draw_idx = 0;
    for (auto& obj : r)
    {
        if (cur_material != obj->material)
        {
            cur_material = obj->material;
            AGEA_check(cur_material, "Shouldn't be null");

            pipeline = cur_material->effect->m_pipeline;
            pipeline_layout = cur_material->effect->m_pipeline_layout;

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            std::array<uint32_t, 2> offsets = {obj_tb.get_offsets()[0],
                                               obj_tb.get_offsets()[1 + cur_material->type_id()]};

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                                    OBJECTS_descriptor_sets, 1, &obj_ds, (uint32_t)offsets.size(),
                                    offsets.data());

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                                    GLOBAL_descriptor_sets, 1, &global_ds,
                                    dyn_tb.get_dyn_offsets_count(), dyn_tb.get_dyn_offsets_ptr());

            if (cur_material->texture_set != VK_NULL_HANDLE)
            {
                // texture descriptor
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                                        TEXTURES_descriptor_sets, 1, &cur_material->texture_set, 0,
                                        nullptr);
            }
        }

        if (cur_mesh != obj->mesh)
        {
            cur_mesh = obj->mesh;
            AGEA_check(cur_mesh, "Should be null");

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &cur_mesh->m_vertexBuffer.buffer(), &offset);

            if (cur_mesh->has_indices())
            {
                vkCmdBindIndexBuffer(cmd, cur_mesh->m_indexBuffer.buffer(), 0,
                                     VK_INDEX_TYPE_UINT32);
            }
        }

        if (!obj->visible)
        {
            continue;
        }

        render::gpu_push_constants c{};
        c.mat_id = cur_material->gpu_idx();

        vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(render::gpu_push_constants), &c);

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
vulkan_render::add_object(render::object_data* obj_data)
{
    AGEA_check(obj_data, "Should be always valid");

    if (obj_data->queue_id == "transparent")
    {
        m_transparent_render_object_queue.emplace_back(obj_data);
    }
    else
    {
        m_default_render_objec_queue[obj_data->queue_id].emplace_back(obj_data);
    }
}

void
vulkan_render::drop_object(render::object_data* obj_data)
{
    AGEA_check(obj_data, "Should be always valid");

    const std::string id = obj_data->queue_id;

    auto& bucket = m_default_render_objec_queue[id];

    auto itr = bucket.find(obj_data);

    AGEA_check(itr == bucket.end(), "Dropping from missing bucket");

    bucket.swap_and_remove(itr);

    if (bucket.get_size() == 0)
    {
        ALOG_TRACE("Dropping old queue");
        m_default_render_objec_queue.erase(id);
    }
}

void
vulkan_render::schedule_material_data_gpu_transfer(render::material_data* md)
{
    for (auto& q : m_transfer_queue)
    {
        q.m_materias_queue_set[md->type_id()].emplace_back(md);
        q.has_materials = true;
    }
}

void
vulkan_render::schedule_game_data_gpu_transfer(render::object_data* obj_date)
{
    for (auto& q : m_transfer_queue)
    {
        q.m_objects_queue.emplace_back(obj_date);
    }
}

void
vulkan_render::update_ssbo_data_ranges(render::gpu_data_index_type range_id)
{
    if (m_ssbo_range.get_size() < (range_id + 1U))
    {
        for (auto& q : m_transfer_queue)
        {
            q.m_materias_queue_set.emplace_back();
        }
        m_ssbo_range.emplace_back(
            std::pair<render::gpu_data_index_type, uint32_t>{range_id, 16 * 1024});
    }
}

void
vulkan_render::update_gpu_object_data(render::gpu_object_data* object_SSBO)
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

    to_update.clear();
}

void
vulkan_render::update_gpu_materials_data(uint8_t* ssbo_data, materials_update_queue& mats_to_update)
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

gpu_transfer_data&
vulkan_render::get_current_frame_transfer_data()
{
    return m_transfer_queue[glob::render_device::getr().get_current_frame_index()];
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

}  // namespace agea
