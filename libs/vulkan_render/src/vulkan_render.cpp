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

const uint32_t INITIAL_OBJECTS_RANGE_SIZE = 4 * 1024;
const uint32_t INITIAL_MATERIAL_RANGE_SIZE = 1 * 1024;

const uint32_t SSBO_BUFFER_SIZE = 16 * 1024;
const uint32_t DYNAMIC_BUFFER_SIZE = 1 * 1024;
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
    m_ssbo_range.emplace_back(INITIAL_OBJECTS_RANGE_SIZE);

    auto device = glob::render_device::get();
    m_frames.resize(device->frame_size());

    for (size_t i = 0; i < m_frames.size(); ++i)
    {
        m_frames[i].frame = &device->frame(i);

        m_frames[i].m_object_buffer = device->create_buffer(
            SSBO_BUFFER_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

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

    if (get_current_frame_transfer_data().has_data())
    {
        upload_render_data(current_frame);
    }

    m_scene_parameters.lights_color = glm::vec4{1.f, 1.f, 1.f, 0.f};
    m_scene_parameters.lights_position = m_lighsts.front()->obj_pos;

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
                                        current_frame.frame->m_dynamic_descriptor_allocator.get())
        .bind_buffer(0, &dynamic_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .bind_buffer(1, &dynamic_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                     VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(global_set);

    for (auto& r : m_default_render_object_queue)
    {
        draw_objects_queue(r.second, cmd, current_frame.m_object_buffer, dyn, global_set,
                           current_frame);
    }

    if (!m_transparent_render_object_queue.empty())
    {
        update_transparent_objects_queue();
        draw_objects_queue(m_transparent_render_object_queue, cmd, current_frame.m_object_buffer,
                           dyn, global_set, current_frame);
    }
}

void
vulkan_render::upload_render_data(render::frame_state& frame)
{
    auto& buffer_td = frame.m_object_buffer;
    auto old_total_size = ssbo_size();

    bool rearranged = rearrange_ssbo();
    if (rearranged)
    {
        auto total_size = ssbo_size();
        if (total_size >= buffer_td.get_alloc_size())
        {
            ALOG_INFO("Reallocating SSBO buffer, partisions {2}, size {0}=>{1}", old_total_size,
                      total_size * 2, m_ssbo_range.get_size());

            frame.m_object_buffer = glob::render_device::getr().create_buffer(
                total_size * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            buffer_td.begin();

            auto gpu_object_data_begin =
                (render::gpu_object_data*)buffer_td.allocate_data(m_ssbo_range.front());

            for (auto& o : glob::vulkan_render_loader::getr().get_objects_cache())
            {
                gpu_object_data_begin[o.second->gpu_index()] = o.second->gpu_data;
            }

            for (size_t i = 1; i < m_ssbo_range.get_size(); ++i)
            {
                auto gpu_material_data_begin = buffer_td.allocate_data(m_ssbo_range[i]);
                for (auto& o : glob::vulkan_render_loader::getr().get_materials_cache())
                {
                    if (o.second->gpu_type_idx() == (i - 1))
                    {
                        auto dst_ptr = gpu_material_data_begin +
                                       o.second->gpu_idx() * o.second->gpu_data.size();
                        memcpy(dst_ptr, o.second->gpu_data.data(), o.second->gpu_data.size());
                    }
                }
            }
            buffer_td.end();
        }
    }

    buffer_td.begin();

    auto gpu_object_data_begin =
        (render::gpu_object_data*)buffer_td.allocate_data(m_ssbo_range.front());

    upload_gpu_object_data(gpu_object_data_begin);

    for (size_t i = 1; i < m_ssbo_range.get_size(); ++i)
    {
        auto gpu_material_data_begin = buffer_td.allocate_data(m_ssbo_range[i]);
        auto& mat_set = get_current_frame_transfer_data().m_materias_queue_set[i - 1];
        upload_gpu_materials_data(gpu_material_data_begin, mat_set);
    }
    buffer_td.end();
}

void
vulkan_render::draw_objects_queue(render_line_conteiner& r,
                                  VkCommandBuffer cmd,
                                  vk_utils::vulkan_buffer& ssbo_buffer,
                                  vk_utils::vulkan_buffer& dyn_buffer,
                                  VkDescriptorSet global_ds,
                                  render::frame_state& current_frame)

{
    const uint32_t dummy_offest[] = {0, 0};

    uint32_t cur_material_idx = INVALID_GPU_INDEX;
    mesh_data* cur_mesh = nullptr;
    material_data* cur_material = nullptr;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    VkDescriptorBufferInfo object_buffer_info{};
    object_buffer_info.buffer = current_frame.m_object_buffer.buffer();
    object_buffer_info.offset = 0;
    object_buffer_info.range = ssbo_buffer.get_range_alloc_size(0);

    uint32_t objects_to_draw_idx = 0;
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

            auto mat_ssbo_range = cur_material->gpu_type_idx() + 1;

            VkDescriptorBufferInfo mat_buffer_info{};
            mat_buffer_info.buffer = current_frame.m_object_buffer.buffer();
            mat_buffer_info.offset = ssbo_buffer.get_offsets()[mat_ssbo_range];
            mat_buffer_info.range = ssbo_buffer.get_range_alloc_size(mat_ssbo_range);

            VkDescriptorSet object_data_set{};
            vk_utils::descriptor_builder::begin(
                glob::render_device::getr().descriptor_layout_cache(),
                current_frame.frame->m_dynamic_descriptor_allocator.get())
                .bind_buffer(0, &object_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                             VK_SHADER_STAGE_VERTEX_BIT)
                .bind_buffer(1, &mat_buffer_info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                             VK_SHADER_STAGE_FRAGMENT_BIT)
                .build(object_data_set);

            AGEA_check(object_data_set, "Should never happens");

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                                    OBJECTS_descriptor_sets, 1, &object_data_set, 2, dummy_offest);

            vkCmdBindDescriptorSets(
                cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, GLOBAL_descriptor_sets, 1,
                &global_ds, dyn_buffer.get_dyn_offsets_count(), dyn_buffer.get_dyn_offsets_ptr());

            for (auto& ts : cur_material->texture_samples)
            {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                                        TEXTURES_descriptor_sets, 1, &ts.descriptor_set, 0,
                                        nullptr);
            }
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

        render::gpu_push_constants c{};
        c.mat_id = obj->material->gpu_idx();

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
        m_default_render_object_queue[obj_data->queue_id].emplace_back(obj_data);
    }
}

void
vulkan_render::drop_object(render::object_data* obj_data)
{
    AGEA_check(obj_data, "Should be always valid");

    const std::string id = obj_data->queue_id;

    auto& bucket = m_default_render_object_queue[id];

    auto itr = bucket.find(obj_data);

    AGEA_check(itr == bucket.end(), "Dropping from missing bucket");

    bucket.swap_and_remove(itr);

    if (bucket.get_size() == 0)
    {
        ALOG_TRACE("Dropping old queue");
        m_default_render_object_queue.erase(id);
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
vulkan_render::add_point_light_source(render::ligh_data* p)
{
    m_lighsts.push_back(p);
}

gpu_data_index_type
vulkan_render::generate_material_ssbo_data_range(const utils::id& mat_id, uint64_t size)
{
    const auto [it, sucess] = m_type_id_range_id.insert(std::make_pair(
        mat_id, material_type_property{(gpu_data_index_type)m_type_id_range_id.size(), size}));

    while (m_ssbo_range.get_size() <= m_type_id_range_id.size())
    {
        for (auto& q : m_frames)
        {
            q.m_materias_queue_set.emplace_back();
        }
        m_ssbo_range.emplace_back(INITIAL_MATERIAL_RANGE_SIZE);
    }

    return it->second.type_index;
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

    to_update.clear();
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
                                                        "root.apkg/class/shader_effects");

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

    auto sampler = glob::vulkan_render_loader::getr().get_sampler_data(AID("font"));

    VkDescriptorImageInfo image_buffer_info{};
    image_buffer_info.sampler = sampler->m_sampler;
    image_buffer_info.imageView = m_ui_txt->image_view;
    image_buffer_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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

    for (auto& ts : m_ui_mat->texture_samples)
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ui_se->m_pipeline_layout,
                                ts.slot, 1, &ts.descriptor_set, 0, nullptr);
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

bool
vulkan_render::rearrange_ssbo()
{
    bool rearanged = false;

    if (m_ssbo_range.front() <= glob::vulkan_render_loader::getr().get_objects_alloc_size())
    {
        ALOG_INFO("Reallocing objects SSBO range {0}=>{1}", m_ssbo_range.front(),
                  glob::vulkan_render_loader::getr().get_objects_alloc_size());
        m_ssbo_range.front() = glob::vulkan_render_loader::getr().get_objects_alloc_size() * 2;
        rearanged = true;
    }

    for (size_t i = 1; i < m_ssbo_range.get_size(); ++i)
    {
        auto mat_size = get_mat_alloc_size(i);
        if (m_ssbo_range[i] <= mat_size)
        {
            ALOG_INFO("Reallocing {0} SSBO range {1}=>{1}", i - 1, m_ssbo_range[i], mat_size);
            m_ssbo_range[i] = mat_size * 2;
        }
        rearanged = true;
    }

    return rearanged;
}

VkDeviceSize
vulkan_render::get_mat_alloc_size(gpu_data_index_type mat_type_index)
{
    for (auto& [i, property] : m_type_id_range_id)
    {
        if (property.type_index = mat_type_index)
        {
            return glob::vulkan_render_loader::getr().last_mat_index(i) * property.alloc_size;
        }
    }

    return {};
}

}  // namespace render
}  // namespace agea