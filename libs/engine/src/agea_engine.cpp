#include "engine/agea_engine.h"

#include "resource_locator/resource_locator.h"
#include "engine/ui.h"
#include "engine/input_manager.h"
#include "engine/editor.h"
#include "engine/config.h"

#include <vulkan_render_types/vulkan_initializers.h>
#include <vulkan_render_types/vulkan_texture_data.h>
#include <vulkan_render_types/vulkan_types.h>
#include <vulkan_render_types/vulkan_material_data.h>
#include <vulkan_render_types/vulkan_shader_data.h>
#include <vulkan_render_types/vulkan_mesh_data.h>
#include <vulkan_render_types/vulkan_shader_effect_data.h>
#include <vulkan_render_types/vulkan_render_data.h>

#include <vulkan_render/render_device.h>
#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vk_descriptors.h>

#include <model/caches/components_cache.h>
#include <model/caches/materials_cache.h>
#include <model/caches/meshes_cache.h>
#include <model/caches/objects_cache.h>
#include <model/caches/textures_cache.h>
#include <model/caches/game_objects_cache.h>
#include <model/caches/caches_map.h>
#include <model/caches/empty_objects_cache.h>
#include <model/reflection/lua_api.h>

#include <model/components/mesh_component.h>
#include <model/game_object.h>
#include <model/assets/shader_effect.h>
#include <model/level_constructor.h>
#include <model/level.h>
#include <model/package_manager.h>

#include <native/native_window.h>
#include <utils/agea_log.h>
#include <utils/process.h>
#include <utils/clock.h>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <SDL_events.h>

#include <imgui.h>

#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_vulkan.h>
#include <VkBootstrap.h>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <thread>

namespace agea
{
glob::engine::type glob::engine::type::s_instance;

namespace
{
const uint32_t GLOBAL_descriptor_sets = 0;
const uint32_t OBJECTS_descriptor_sets = 1;
const uint32_t TEXTURES_descriptor_sets = 2;
}  // namespace

vulkan_engine::vulkan_engine(std::unique_ptr<singleton_registry> r)
    : m_registry(std::move(r))
    , m_scene(std::make_unique<scene_builder>())
{
}
void
stupid_sleep(std::chrono::microseconds sleep_for)
{
    auto current = std::chrono::high_resolution_clock::now();
    auto to = current + sleep_for;
    auto dt = to - current;

    while (dt > std::chrono::microseconds(500) && current < to)
    {
        dt /= 2;

        std::this_thread::sleep_for(dt);
        current = std::chrono::high_resolution_clock::now();
        dt = to - current;
    }
}

vulkan_engine::~vulkan_engine()
{
}

bool
vulkan_engine::init()
{
    ALOG_INFO("Initialization started ...");

    glob::game_editor::create(*m_registry);
    glob::input_manager::create(*m_registry);
    glob::resource_locator::create(*m_registry);
    glob::config::create(*m_registry);
    glob::render_device::create(*m_registry);
    glob::vulkan_render_loader::create(*m_registry);
    glob::ui::create(*m_registry);
    glob::level::create(*m_registry);
    glob::native_window::create(*m_registry);
    glob::package_manager::create(*m_registry);
    glob::lua_api::create(*m_registry);

    glob::init_global_caches(*m_registry);

    glob::resource_locator::get()->init_local_dirs();
    auto cfgs_folder = glob::resource_locator::get()->resource_dir(category::configs);

    utils::path main_config = cfgs_folder / "agea.acfg";
    glob::config::get()->load(main_config);

    utils::path input_config = cfgs_folder / "inputs.acfg";
    glob::input_manager::get()->load_actions(main_config);

    ::agea::reflection::entry::set_up();

    glob::game_editor::get()->init();

    native_window::construct_params rwc;
    rwc.w = 1600 * 2;
    rwc.h = 900 * 2;
    auto window = glob::native_window::get();

    if (!window->construct(rwc))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    render::render_device::construct_params rdc;
    rdc.window = window->handle();

    auto device = glob::render_device::get();
    if (!device->construct(rdc))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    m_transfer_queue.resize(device->frame_size());

    init_scene();

    ALOG_INFO("Initialization completed");
    return true;
}
void
vulkan_engine::cleanup()
{
    glob::render_device::get()->wait_for_fences();

    glob::vulkan_render_loader::get()->clear_caches();

    glob::render_device::get()->destruct();
}

void
vulkan_engine::draw()
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

    ::agea::glob::ui::get()->draw(cmd);

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
vulkan_engine::run()
{
    float frame_time = 1.f / glob::config::get()->fps_lock;
    const std::chrono::microseconds frame_time_int(1000000 / glob::config::get()->fps_lock);

    // main loop
    for (;;)
    {
        auto start_ts = utils::get_current_time_mks();

        if (!glob::input_manager::get()->input_tick(frame_time))
        {
            break;
        }

        glob::input_manager::get()->input_tick(frame_time);
        glob::game_editor::get()->on_tick(frame_time);

        update_cameras();

        glob::ui::get()->new_frame();

        tick(frame_time);

        consume_updated_shader_effects();
        consume_updated_render_assets();
        consume_updated_render_components();
        consume_updated_transforms();

        draw();

        glob::vulkan_render_loader::getr().delete_sheduled_actions();

        auto frame_msk = std::chrono::microseconds(utils::get_current_time_mks() - start_ts);

        if (frame_msk < frame_time_int)
        {
            stupid_sleep(std::chrono::microseconds(frame_time_int - frame_msk));
        }

        frame_msk = std::chrono::microseconds(utils::get_current_time_mks() - start_ts);
        frame_time = 0.00001f * frame_msk.count();
    }
}

void
vulkan_engine::tick(float dt)
{
    glob::level::get()->tick(dt);
}

void
vulkan_engine::update_gpu_object_data(render::gpu_object_data* object_SSBO)
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
vulkan_engine::update_gpu_materials_data(uint8_t* ssbo_data, materials_update_queue& mats_to_update)
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

bool
vulkan_engine::load_level(const utils::id& level_id)
{
    auto result = model::level_constructor::load_level_id(
        *glob::level::get(), glob::config::get()->level, glob::class_objects_cache_set_view::getr(),
        glob::objects_cache_set_view::getr());

    if (!result)
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    if (!prepare_for_rendering(*glob::level::get()))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    if (!schedule_for_rendering(*glob::level::get()))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    return true;
}

void
vulkan_engine::update_transparent_objects_queue()
{
    for (auto& obj : m_transparent_render_object_queue)
    {
        obj->distance_to_camera = glm::length(obj->gpu_data.obj_pos - m_camera_data.position);
    }

    std::sort(m_transparent_render_object_queue.begin(), m_transparent_render_object_queue.end(),
              [](render::object_data* l, render::object_data* r)
              { return l->distance_to_camera > r->distance_to_camera; });
}

vulkan_engine::gpu_transfer_data&
vulkan_engine::get_current_frame_transfer_data()
{
    return m_transfer_queue[glob::render_device::getr().get_current_frame_index()];
}

void
vulkan_engine::consume_updated_transforms()
{
    auto& items = glob::level::getr().get_dirty_transforms_components_queue();

    if (items.empty())
    {
        return;
    }

    for (auto& i : items)
    {
        auto obj_data = i->get_object_dat();
        if (obj_data)
        {
            obj_data->gpu_data.model_matrix = i->get_transofrm_matrix();
            i->set_dirty_transform(false);
            schedule_game_data_gpu_transfer(obj_data);
        }
    }

    items.clear();
}

void
vulkan_engine::draw_new_objects(VkCommandBuffer cmd, render::frame_data& current_frame)
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

    auto c = glob::level::getr()
                 .find_component(AID("point_light__light_component"))
                 ->as<model::game_object_component>();

    m_scene_parameters.lights_color = glm::vec4{1.f, 1.f, 1.f, 0.f};
    m_scene_parameters.lights_position =
        c ? c->get_world_position() : glm::vec4{1.f, 1.f, 1.f, 0.f};

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
vulkan_engine::draw_objects(render_line_conteiner& r,
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

        if (!obj->visible)
        {
            continue;
        }

        render::gpu_push_constants c{};
        c.mat_id = cur_material->gpu_idx();

        vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(render::gpu_push_constants), &obj->material->gpu_data);

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
vulkan_engine::add_object(render::object_data* obj_data)
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
vulkan_engine::drop_object(render::object_data* obj_data)
{
    AGEA_check(obj_data, "Should be always valid");

    const std::string id = obj_data->queue_id;

    auto& bucket = m_default_render_objec_queue[id];

    auto pos = bucket.find(obj_data);

    AGEA_check(pos == bucket.end(), "Dropping from missing bucket");

    bucket.swap_and_remove(pos);

    if (bucket.get_size() == 0)
    {
        ALOG_TRACE("Dropping old queu");
        m_default_render_objec_queue.erase(id);
    }
}

void
vulkan_engine::schedule_material_data_gpu_transfer(render::material_data* md)
{
    for (auto& q : m_transfer_queue)
    {
        q.m_materias_queue_set[md->type_id()].emplace_back(md);
        q.has_materials = true;
    }
}

void
vulkan_engine::schedule_game_data_gpu_transfer(render::object_data* obj_date)
{
    for (auto& q : m_transfer_queue)
    {
        q.m_objects_queue.emplace_back(obj_date);
    }
}

void
vulkan_engine::update_ssbo_data_ranges(render::gpu_data_index_type range_id)
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
vulkan_engine::update_cameras()
{
    m_camera_data = glob::game_editor::get()->get_camera_data();
}

void
vulkan_engine::init_scene()
{
    load_level(glob::config::get()->level);
}

void
vulkan_engine::consume_updated_render_components()
{
    auto& items = glob::level::getr().get_dirty_render_queue();

    for (auto& i : items)
    {
        auto obj_data = (render::object_data*)i->get_object_dat();

        if (obj_data)
        {
            m_scene->prepare_for_rendering(*i, false);
            m_scene->schedule_for_rendering(*i, false);
        }
    }

    items.clear();
}

void
vulkan_engine::consume_updated_render_assets()
{
    auto& items = glob::level::getr().get_dirty_render_assets_queue();

    for (auto& i : items)
    {
        m_scene->prepare_for_rendering(*i, false);
    }

    items.clear();
}

void
vulkan_engine::consume_updated_shader_effects()
{
    auto& items = glob::level::getr().get_dirty_shader_effect_queue();

    for (auto& i : items)
    {
        m_scene->prepare_for_rendering(*i, false);
    }

    items.clear();
}

bool
vulkan_engine::prepare_for_rendering(model::package& p)
{
    auto& cs = p.get_objects();

    for (auto& o : cs)
    {
        if (!m_scene->prepare_for_rendering(*o, true))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    return true;
}

bool
vulkan_engine::prepare_for_rendering(model::level& p)
{
    auto& cs = p.get_game_objects();

    for (auto& o : cs.get_items())
    {
        if (!m_scene->prepare_for_rendering(*o.second, true))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    return true;
}

bool
vulkan_engine::schedule_for_rendering(model::level& p)
{
    auto& cs = p.get_game_objects();

    for (auto& o : cs.get_items())
    {
        if (!m_scene->schedule_for_rendering(*o.second, true))
        {
            ALOG_LAZY_ERROR;
            return false;
        }
    }

    return true;
}

void
vulkan_engine::event_reload_world()
{
    AGEA_never("Not Implemented!");
}

void
vulkan_engine::compile_all_shaders()
{
    //     auto shader_dir = glob::resource_locator::get()->resource_dir(category::shaders_raw);
    //     auto shader_compiled_dir =
    //         glob::resource_locator::get()->resource_dir(category::shaders_compiled);
    //
    //     auto stamp = shader_compiled_dir.fs() / "nice";
    //     if (std::filesystem::exists(stamp) && !m_force_shader_recompile)
    //     {
    //         return;
    //     }
    //
    //     for (auto& p : std::filesystem::recursive_directory_iterator(shader_dir.fs()))
    //     {
    //         if (p.is_directory())
    //         {
    //             continue;
    //         }
    //
    //         auto shader_path = std::filesystem::relative(p, shader_dir.fs());
    //
    //         ipc::construct_params params;
    //         params.path_to_binary = "C:\\VulkanSDK\\1.2.170.0\\Bin\\glslc.exe";
    //
    //         auto td = glob::resource_locator::get()->temp_dir();
    //         params.working_dir = *td.folder;
    //
    //         auto raw_path = shader_dir.fs() / shader_path;
    //         auto compiled_path = *td.folder / shader_path;
    //         compiled_path += ".spv";
    //
    //         auto final_path = shader_compiled_dir.fs() / shader_path;
    //         final_path += ".spv";
    //
    //         params.arguments =
    //             "-V " + raw_path.generic_string() + " -o " + compiled_path.generic_string();
    //
    //         uint64_t rc = 0;
    //         if (!ipc::run_binary(params, rc))
    //         {
    //             AGEA_never("Shader compilation failed");
    //
    //             return;
    //         }
    //
    //         auto name = shader_path.generic_string() + ".spv";
    //
    //         std::filesystem::rename(compiled_path, final_path);
    //     }
    //
    //     std::ofstream file(stamp);
    //     file << "hehey)";

    return;
}

}  // namespace agea
