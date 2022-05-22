#include "core/vk_engine.h"

#include "core/fs_locator.h"

#include "core/vk_initializers.h"
#include "core/vk_descriptors.h"

#include "vulkan_render/vulkan_texture_data.h"
#include "vulkan_render/vulkan_types.h"
#include "vulkan_render/vulkan_material_data.h"
#include "vulkan_render/vulkan_shader_data.h"
#include "vulkan_render/vulkan_mesh_data.h"
#include "vulkan_render/vulkan_shader_effect.h"
#include "vulkan_render/vulkan_render_data.h"
#include "vulkan_render/render_device.h"
#include "vulkan_render/render_loader.h"

#include "model/caches/textures_cache.h"
#include "model/caches/materials_cache.h"
#include "model/caches/meshes_cache.h"
#include "model/caches/class_object_cache.h"

#include "model/components/mesh_component.h"
#include "model/level_constructor.h"
#include "model/level.h"

#include "ui/ui.h"
#include "editor/cli/cli.h"
#include "utils/process.h"
#include "core/native_window.h"

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

#include "utils/agea_log.h"

namespace agea
{

vulkan_engine::vulkan_engine()
    : m_current_level()
    , m_window()
    , m_resource_locator()
    , m_render_loader()
    , m_render_device()
{
}

vulkan_engine::~vulkan_engine()
{
}

bool
vulkan_engine::init()
{
    ALOG_INFO("Initialization started ...");

    m_current_level = glob::level::create();
    m_window = glob::native_window::create();
    m_resource_locator = glob::resource_locator::create();
    m_render_loader = glob::render_loader::create();
    m_render_device = glob::render_device::create();
    m_class_objects_cache = glob::class_objects_cache::create();

    m_ui = glob::ui::create();
    m_editor_cli = glob::cli::create();

    glob::resource_locator::get()->init_local_dirs();

    native_window::construct_params rwc;
    rwc.w = 1600 * 2;
    rwc.h = 900 * 2;
    auto window = glob::native_window::get();

    if (!window->construct(rwc))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    m_force_shader_recompile = true;
    compile_all_shaders();

    render::render_device::construct_params rdc;
    rdc.window = window->handle();

    auto device = glob::render_device::get();

    if (!device->construct(rdc))
    {
        ALOG_LAZY_ERROR;
        return false;
    }

    init_scene();

    m_camera = {};
    m_camera.position = {30.f, 0.f, 50.f};

    ALOG_INFO("Initialization completed");
    return true;
}
void
vulkan_engine::cleanup()
{
    glob::render_device::get()->wait_for_fences();

    glob::render_loader::get()->clear_caches();

    glob::render_device::get()->destruct();
}

void
vulkan_engine::handle_dirty_objects()
{
    for (auto o : m_qs.dirty_objects())
    {
        m_qs.remove_from_rdc(o);

        o->prepare_for_rendering();

        m_qs.add_to_queue(o);
    }
    m_qs.clear_dirty_queue();
}

void
vulkan_engine::draw()
{
    auto device = glob::render_device::get();

    auto& current_frame = get_current_frame();

    // wait until the gpu has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(
        vkWaitForFences(device->vk_device(), 1, &current_frame.m_render_fence, true, 1000000000));
    VK_CHECK(vkResetFences(device->vk_device(), 1, &current_frame.m_render_fence));

    ImGui::Render();
    get_current_frame().m_dynamic_descriptor_allocator->reset_pools();

    // now that we are sure that the commands finished executing, we can safely reset the command
    // buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(current_frame.m_main_command_buffer, 0));

    // request image from the swapchain
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(device->vk_device(), device->swapchain(), 1000000000,
                                   current_frame.m_present_semaphore, nullptr,
                                   &swapchainImageIndex));

    // naming it cmd for shorter writing
    VkCommandBuffer cmd = get_current_frame().m_main_command_buffer;

    // begin the command buffer recording. We will use this command buffer exactly once, so we want
    // to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo =
        vk_init::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

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
        vk_init::renderpass_begin_info(device->render_pass(), VkExtent2D{width, height},
                                       device->framebuffers(swapchainImageIndex));

    // connect clear values
    rpInfo.clearValueCount = 2;

    VkClearValue clearValues[] = {clearValue, depthClear};

    rpInfo.pClearValues = &clearValues[0];

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    draw_new_objects(cmd);

    ::agea::glob::ui::get()->draw(cmd);

    // finalize the render pass
    vkCmdEndRenderPass(cmd);
    // finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    // prepare the submission to the queue.
    // we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is
    // ready we will signal the _renderSemaphore, to signal that rendering has finished

    VkSubmitInfo submit = vk_init::submit_info(&cmd);
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
    VkPresentInfoKHR presentInfo = vk_init::present_info();

    presentInfo.pSwapchains = &device->swapchain();
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &get_current_frame().m_render_semaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(device->vk_graphics_queue(), &presentInfo));

    // increase the number of frames drawn
    m_frame_number++;
}

void
vulkan_engine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit)
    {
        handle_dirty_objects();

        // Handle events on queue
        while (SDL_PollEvent(&e) != 0)
        {
            ImGui_ImplSDL2_ProcessEvent(&e);

            ImGuiIO& io = ImGui::GetIO();
            if (!io.WantCaptureKeyboard)
            {
                process_input_event(&e);
            }

            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
            {
                bQuit = true;
            }
        }

        glob::ui::get()->new_frame();

        update_camera(1.0f / 60.0f);

        glob::level::get()->update();

        draw();
    }
}

render::frame_data&
vulkan_engine::get_current_frame()
{
    return glob::render_device::get()->frame(m_frame_number % FRAMES_IN_FLYIGNT);
}

void
vulkan_engine::process_input_event(SDL_Event* ev)
{
    if (ev->type == SDL_KEYDOWN)
    {
        m_pressed[ev->key.keysym.sym] = true;

        if (m_pressed[SDLK_LCTRL])
        {
            if (m_pressed[SDLK_r])
            {
                event_reload_world();
                return;
            }
        }

        switch (ev->key.keysym.sym)
        {
        case SDLK_UP:
        case SDLK_w:
            m_camera.inputAxis.x += 1.f;
            break;
        case SDLK_DOWN:
        case SDLK_s:
            m_camera.inputAxis.x -= 1.f;
            break;
        case SDLK_LEFT:
        case SDLK_a:
            m_camera.inputAxis.y -= 1.f;
            break;
        case SDLK_RIGHT:
        case SDLK_d:
            m_camera.inputAxis.y += 1.f;
        }
    }
    else if (ev->type == SDL_KEYUP)
    {
        m_pressed[ev->key.keysym.sym] = false;

        switch (ev->key.keysym.sym)
        {
        case SDLK_UP:
        case SDLK_w:
            m_camera.inputAxis.x -= 1.f;
            break;
        case SDLK_DOWN:
        case SDLK_s:
            m_camera.inputAxis.x += 1.f;
            break;
        case SDLK_LEFT:
        case SDLK_a:
            m_camera.inputAxis.y += 1.f;
            break;
        case SDLK_RIGHT:
        case SDLK_d:
            m_camera.inputAxis.y -= 1.f;
            break;
        }
    }
    else if (ev->type == SDL_MOUSEMOTION)
    {
        // m_camera.pitch -= ev->motion.yrel * 0.003;
        // m_camera.yaw -= ev->motion.xrel * 0.003;
    }

    m_camera.inputAxis = glm::clamp(m_camera.inputAxis, {-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0});
}

void
vulkan_engine::update_camera(float deltaSeconds)
{
    glm::vec3 forward = {0, 0, -1};
    glm::vec3 right = {1, 0, 0};

    glm::mat4 cam_rot = m_camera.get_rotation_matrix();

    forward = cam_rot * glm::vec4(forward, 0.f);
    right = cam_rot * glm::vec4(right, 0.f);

    m_camera.velocity = m_camera.inputAxis.x * forward + m_camera.inputAxis.y * right;

    m_camera.velocity *= 10 * deltaSeconds;

    m_camera.position += m_camera.velocity;
}

void
vulkan_engine::draw_new_objects(VkCommandBuffer cmd)
{
    // make a model view matrix for rendering the object
    // camera view
    glm::vec3 camPos = m_camera.position;
    glm::mat4 cam_rot = m_camera.get_rotation_matrix();
    glm::mat4 view = glm::translate(glm::mat4{1}, camPos) * cam_rot;

    // we need to invert the camera matrix
    view = glm::inverse(view);

    auto w = (float)glob::native_window::get()->get_size().w;
    auto h = (float)glob::native_window::get()->get_size().h;

    // camera projection
    glm::mat4 projection = glm::perspective(glm::radians(70.f), w / h, 0.1f, 2000.0f);
    projection[1][1] *= -1;

    render::gpu_camera_data camData{};
    camData.projection = projection;
    camData.view = view;
    camData.pos = camPos;

    float framed = (m_frame_number / 120.f);
    m_scene_parameters.ambient_color = {sin(framed), 0, cos(framed), 1};

    auto device = glob::render_device::get();

    // Upload object matrices
    void* objectData = nullptr;
    vmaMapMemory(device->allocator(), get_current_frame().m_object_buffer.allocation(),
                 &objectData);

    auto objectSSBO = (render::gpu_object_data*)objectData;

    uint32_t objects_to_draw_count = 0;
    for (auto& r : m_qs.queues())
    {
        for (auto obj : r.second)
        {
            objectSSBO[objects_to_draw_count++].model_matrix = obj->transform_matrix;
        }
    }

    vmaUnmapMemory(device->allocator(), get_current_frame().m_object_buffer.allocation());

    VkDescriptorBufferInfo objectBufferInfo{};
    objectBufferInfo.buffer = get_current_frame().m_object_buffer.buffer();
    objectBufferInfo.offset = 0;
    objectBufferInfo.range = sizeof(render::gpu_object_data) * objects_to_draw_count;

    VkDescriptorSet ObjectDataSet;
    vk_utils::descriptor_builder::begin(device->descriptor_layout_cache(),
                                        get_current_frame().m_dynamic_descriptor_allocator.get())
        .bind_buffer(0, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     VK_SHADER_STAGE_VERTEX_BIT)
        .build(ObjectDataSet);

    // push data to dynmem
    uint32_t camera_data_offsets = 0;
    uint32_t dyn_offset = 0;

    // Global part
    char* dynData = nullptr;
    vmaMapMemory(device->allocator(), get_current_frame().m_dynamic_data_buffer.allocation(),
                 (void**)&dynData);

    {
        camera_data_offsets = dyn_offset;
        memcpy(dynData, &camData, sizeof(render::gpu_camera_data));
        dyn_offset += sizeof(render::gpu_camera_data);
        dyn_offset = device->pad_uniform_buffer_size(dyn_offset);

        dynData += dyn_offset;
    }

    auto scene_data_offset = dyn_offset;

    float p = 30.f;

    m_scene_parameters.lights[0] = glm::vec4(-p, -p * 0.5f, -p, 0.01f);
    m_scene_parameters.lights[1] = glm::vec4(-p, -p * 0.5f, p, 0.01f);
    m_scene_parameters.lights[2] = glm::vec4(p, -p * 0.5f, p, 0.01f);
    m_scene_parameters.lights[3] = glm::vec4(p, -p * 0.5f, -p, 0.01f);

    memcpy(dynData, &m_scene_parameters, sizeof(render::gpu_scene_data));

    vmaUnmapMemory(device->allocator(), get_current_frame().m_dynamic_data_buffer.allocation());
    ///
    dyn_offset += sizeof(render::gpu_scene_data);
    dyn_offset = device->pad_uniform_buffer_size(dyn_offset);

    VkDescriptorBufferInfo dynamicInfo{};
    dynamicInfo.buffer = get_current_frame().m_dynamic_data_buffer.buffer();
    dynamicInfo.offset = 0;
    dynamicInfo.range = dyn_offset;

    VkDescriptorSet GlobalSet;
    vk_utils::descriptor_builder::begin(device->descriptor_layout_cache(),
                                        get_current_frame().m_dynamic_descriptor_allocator.get())
        .bind_buffer(0, &dynamicInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .bind_buffer(1, &dynamicInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                     VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(GlobalSet);

    uint32_t objects_to_draw_idx = 0;
    for (auto& r : m_qs.queues())
    {
        auto cur_mesh = r.second.front()->mesh;
        auto cur_material = r.second.front()->material;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cur_material->pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                cur_material->effect->m_build_layout, 1, 1, &ObjectDataSet, 0,
                                nullptr);

        // update dynamic binds
        uint32_t dynamicBinds[] = {camera_data_offsets, scene_data_offset};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                cur_material->effect->m_build_layout, 0, 1, &GlobalSet, 2,
                                dynamicBinds);

        if (cur_material->texture_set != VK_NULL_HANDLE)
        {
            // texture descriptor
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    cur_material->effect->m_build_layout, 2, 1,
                                    &cur_material->texture_set, 0, nullptr);
        }

        // only bind the mesh if its a different one from last bind
        // bind the mesh vertex buffer with offset 0
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &cur_mesh->m_vertexBuffer.buffer(), &offset);

        if (cur_mesh->has_indices())
        {
            vkCmdBindIndexBuffer(cmd, cur_mesh->m_indexBuffer.buffer(), 0, VK_INDEX_TYPE_UINT32);
        }

        for (auto obj : r.second)
        {
            if (!obj->visible)
            {
                continue;
            }

            /// TODO, rework!
            glm::mat4 model = obj->transform_matrix;
            // final render matrix, that we are calculating on the cpu
            glm::mat4 mesh_matrix = model;

            //             render::mesh_push_constants constants{};
            //             constants.render_matrix = mesh_matrix;
            //
            // upload the mesh to the gpu via pushconstants
            vkCmdPushConstants(cmd, obj->material->effect->m_build_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(render::gpu_material_data),
                               &obj->material->gpu_data);

            // we can now draw

            if (!obj->mesh->has_indices())
            {
                vkCmdDraw(cmd, (uint32_t)obj->mesh->vertices_size(), 1, 0, objects_to_draw_idx);
            }
            else
            {
                vkCmdDrawIndexed(cmd, (uint32_t)obj->mesh->indices_size(), 1, 0, 0,
                                 objects_to_draw_idx);
            }
            ++objects_to_draw_idx;
        }
    }
}

void
vulkan_engine::init_scene()
{
    m_textures_cache = glob::textures_cache::create();
    glob::textures_cache::get()->init();

    m_materials_cache = glob::materials_cache::create();
    glob::materials_cache::get()->init();

    m_meshes_cache = glob::meshes_cache::create();
    glob::meshes_cache::get()->init();

    model::level_constructor::load_level_id(*glob::level::get(), "demo.plvl");

    for (auto& o : glob::level::get()->m_objects)
    {
        auto obj = o->root_component();
        obj->prepare_for_rendering();
        obj->register_for_rendering();
    }
}

void
vulkan_engine::event_reload_world()
{
    auto level = glob::level::get();
    level->m_objects.clear();

    model::level_constructor::load_level_id(*glob::level::get(), "demo.plvl");

    for (auto& o : glob::level::get()->m_objects)
    {
        auto obj = o->root_component();
        obj->register_for_rendering();
    }
}

void
vulkan_engine::compile_all_shaders()
{
    std::filesystem::path shader_dir =
        glob::resource_locator::get()->resource_dir(category::shaders_raw);
    std::filesystem::path shader_compiled_dir =
        glob::resource_locator::get()->resource_dir(category::shaders_compiled);

    auto stamp = shader_compiled_dir / "nice";
    if (std::filesystem::exists(stamp) && !m_force_shader_recompile)
    {
        return;
    }

    for (auto& p : std::filesystem::recursive_directory_iterator(shader_dir))
    {
        if (p.is_directory())
        {
            continue;
        }

        auto shader_path = std::filesystem::relative(p, shader_dir);

        ipc::construct_params params;
        params.path_to_binary = "C:\\VulkanSDK\\1.2.170.0\\Bin\\glslc.exe";

        auto td = glob::resource_locator::get()->temp_dir();
        params.working_dir = *td.folder;

        auto raw_path = shader_dir / shader_path;
        auto compiled_path = *td.folder / shader_path;
        compiled_path += ".spv";

        auto final_path = shader_compiled_dir / shader_path;
        final_path += ".spv";

        params.arguments =
            "-V " + raw_path.generic_string() + " -o " + compiled_path.generic_string();

        uint64_t rc = 0;
        if (!ipc::run_binary(params, rc))
        {
            AGEA_never("Shader compilation failed");

            return;
        }

        auto name = shader_path.generic_string() + ".spv";

        std::filesystem::rename(compiled_path, final_path);
    }

    std::ofstream file(stamp);
    file << "hehey)";

    return;
}

glm::mat4
player_camera::get_rotation_matrix()
{
    glm::mat4 yaw_rot = glm::rotate(glm::mat4{1}, yaw, {0, 1, 0});
    glm::mat4 pitch_rot = glm::rotate(glm::mat4{yaw_rot}, pitch, {1, 0, 0});

    return pitch_rot;
}

}  // namespace agea
