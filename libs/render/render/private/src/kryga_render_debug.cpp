#include "vulkan_render/kryga_render.h"

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_mesh_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"

#include <gpu_types/gpu_generic_constants.h>

#include <global_state/global_state.h>

#include <tracy/Tracy.hpp>

namespace kryga
{
namespace render
{

void
vulkan_render::draw_debug_lights(VkCommandBuffer cmd, render::frame_state& current_frame)
{
    ZoneScopedN("Render::DrawDebugLights");

    if (!m_debug_light_config.show_wireframe)
        return;

    if (!m_debug_wire_se || !m_debug_wire_mat || m_debug_wire_se->m_failed_load)
        return;

    if (m_cache.universal_lights.get_size() == 0)
        return;

    auto* cube = glob::glob_state().getr_vulkan_render_loader().get_mesh_data(AID("cube_mesh"));
    if (!cube || cube->m_vertex_buffer.buffer() == VK_NULL_HANDLE)
        return;

    // Write temporary debug object_data entries at the end of the objects buffer.
    // Each light gets one entry with a model matrix = translate(pos) * scale(radius_vis).
    uint32_t debug_base_slot = m_cache.objects.get_size();
    uint32_t light_count = 0;

    // Count valid lights
    for (uint32_t i = 0; i < m_cache.universal_lights.get_size(); ++i)
    {
        auto* light = m_cache.universal_lights.at(i);
        if (light && light->is_valid())
            ++light_count;
    }

    if (light_count == 0)
        return;

    // Ensure objects buffer has space for debug entries
    uint32_t required_size =
        (debug_base_slot + light_count) * sizeof(gpu::object_data);
    if (required_size > current_frame.buffers.objects.get_alloc_size())
        return;  // Buffer too small, skip debug drawing

    // Write debug object data
    current_frame.buffers.objects.begin();
    auto* obj_data = reinterpret_cast<gpu::object_data*>(
        current_frame.buffers.objects.get_data());

    // Also build instance slots for debug draws
    std::vector<uint32_t> debug_slots;
    debug_slots.reserve(light_count);

    uint32_t debug_idx = 0;
    for (uint32_t i = 0; i < m_cache.universal_lights.get_size(); ++i)
    {
        auto* light = m_cache.universal_lights.at(i);
        if (!light || !light->is_valid())
            continue;

        uint32_t slot = debug_base_slot + debug_idx;

        // Visual radius: small sphere for position, not the full light radius
        float vis_radius = 0.3f;

        glm::mat4 model = glm::translate(glm::mat4(1.0f), light->gpu_data.position) *
                           glm::scale(glm::mat4(1.0f), glm::vec3(vis_radius));

        obj_data[slot].model = model;
        obj_data[slot].normal = glm::transpose(glm::inverse(model));
        obj_data[slot].obj_pos = light->gpu_data.position;
        obj_data[slot].bounding_radius = vis_radius;
        obj_data[slot].material_id = m_debug_wire_mat->gpu_idx();
        obj_data[slot].bone_offset = 0;
        obj_data[slot].bone_count = 0;

        debug_slots.push_back(slot);
        ++debug_idx;
    }
    current_frame.buffers.objects.end();

    // Write debug instance slots at end of instance_slots buffer
    uint32_t debug_instance_base = static_cast<uint32_t>(m_instance_slots_staging.size());

    // Append debug slots to staging and re-upload
    for (auto s : debug_slots)
        m_instance_slots_staging.push_back(s);
    upload_instance_slots(current_frame);

    // Bind debug wire pipeline
    auto* se = m_debug_wire_se;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, se->m_pipeline);

    // Bind descriptor sets
    auto pipeline_layout = se->m_pipeline_layout;
    const uint32_t dummy_offset[KGPU_objects_max_binding + 1] = {};

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                            KGPU_global_descriptor_sets, 1, &m_global_set,
                            current_frame.buffers.dynamic_data.get_dyn_offsets_count(),
                            current_frame.buffers.dynamic_data.get_dyn_offsets_ptr());

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                            KGPU_objects_descriptor_sets, 1, &m_objects_set,
                            KGPU_objects_max_binding + 1, dummy_offset);

    // Bind bindless textures + material set
    if (m_bindless_set != VK_NULL_HANDLE)
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                                KGPU_textures_descriptor_sets, 1, &m_bindless_set, 0, nullptr);
    }

    const uint32_t mat_dummy = 0;
    VkDescriptorBufferInfo mat_buf_info = {};
    mat_buf_info.buffer = current_frame.buffers.materials.buffer();
    mat_buf_info.offset = 0;
    mat_buf_info.range = VK_WHOLE_SIZE;

    // Bind cube mesh
    bind_mesh(cmd, cube);

    // Draw each light as a wireframe cube
    gpu::push_constants pc = {};
    pc.material_id = m_debug_wire_mat->gpu_idx();
    pc.use_clustered_lighting = 0;
    pc.directional_light_id = 0;

    for (uint32_t i = 0; i < light_count; ++i)
    {
        pc.instance_base = debug_instance_base + i;

        for (uint32_t t = 0; t < KGPU_MAX_TEXTURE_SLOTS; ++t)
        {
            pc.texture_indices[t] = UINT32_MAX;
            pc.sampler_indices[t] = KGPU_SAMPLER_LINEAR_REPEAT;
        }

        vkCmdPushConstants(cmd, se->m_pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(gpu::push_constants), &pc);

        if (cube->has_indices())
            vkCmdDrawIndexed(cmd, cube->indices_size(), 1, 0, 0, 0);
        else
            vkCmdDraw(cmd, cube->vertices_size(), 1, 0, 0);
    }

    // Remove debug slots from staging (clean up for next frame)
    m_instance_slots_staging.resize(debug_instance_base);
}

}  // namespace render
}  // namespace kryga
