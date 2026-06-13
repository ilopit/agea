#include "vulkan_render/kryga_render.h"

#include "vulkan_render/render_system.h"
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
vulkan_render::prepare_debug_light_data(render::frame_state& current_frame)
{
    ZoneScopedN("Render::PrepareDebugLightData");

    m_debug_light_draw_count = 0;
    m_debug_light_instance_base = 0;

    if (!m_render_config.debug.light_wireframe)
    {
        return;
    }

    if (!m_debug_wire_se || !m_debug_wire_mat || m_debug_wire_se->m_failed_load)
    {
        return;
    }

    if (m_loader->uni_lights_size() == 0)
    {
        return;
    }

    // Count valid lights
    uint32_t light_count = 0;
    for (uint32_t i = 0; i < m_loader->uni_lights_size(); ++i)
    {
        auto* light = m_loader->uni_light_at(i);
        if (light && light->is_valid())
        {
            ++light_count;
        }
    }
    if (light_count == 0)
    {
        return;
    }

    // Write debug object_data entries at the end of the objects buffer
    uint32_t debug_base_slot = m_loader->objects_capacity();
    uint32_t required_size = (debug_base_slot + light_count) * sizeof(gpu::object_data);
    if (required_size > current_frame.buffers.objects.get_alloc_size())
    {
        return;
    }

    current_frame.buffers.objects.begin();
    auto* obj_data = reinterpret_cast<gpu::object_data*>(current_frame.buffers.objects.get_data());

    uint32_t debug_idx = 0;
    for (uint32_t i = 0; i < m_loader->uni_lights_size(); ++i)
    {
        auto* light = m_loader->uni_light_at(i);
        if (!light || !light->is_valid())
        {
            continue;
        }

        uint32_t slot = debug_base_slot + debug_idx;
        float radius = light->gpu_data.radius;
        bool is_spot = light->gpu_data.cut_off > 0.0f;

        glm::mat4 model;
        if (is_spot)
        {
            // Cone: apex at light position, opens along light direction.
            // Unit cone opens along -Y, so rotate -Y to match light direction.
            glm::vec3 dir = glm::normalize(light->gpu_data.direction);
            float cone_angle = std::acos(light->gpu_data.outer_cut_off);
            float base_r = std::tan(cone_angle) * radius;

            glm::vec3 from(0.0f, -1.0f, 0.0f);
            glm::vec3 axis = glm::cross(from, dir);
            float axis_len = glm::length(axis);
            glm::mat4 rot(1.0f);
            if (axis_len > 1e-6f)
            {
                float angle = std::acos(glm::clamp(glm::dot(from, dir), -1.0f, 1.0f));
                rot = glm::rotate(glm::mat4(1.0f), angle, glm::normalize(axis));
            }
            else if (glm::dot(from, dir) < 0.0f)
            {
                rot = glm::rotate(glm::mat4(1.0f), glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
            }

            model = glm::translate(glm::mat4(1.0f), light->gpu_data.position) * rot *
                    glm::scale(glm::mat4(1.0f), glm::vec3(base_r, radius, base_r));
        }
        else
        {
            model = glm::translate(glm::mat4(1.0f), light->gpu_data.position) *
                    glm::scale(glm::mat4(1.0f), glm::vec3(radius));
        }

        obj_data[slot].model = model;
        obj_data[slot].normal = glm::transpose(glm::inverse(model));
        obj_data[slot].obj_pos = light->gpu_data.position;
        obj_data[slot].bounding_radius = radius;
        obj_data[slot].material_id = m_debug_wire_mat->gpu_idx();
        obj_data[slot].bone_offset = 0;
        obj_data[slot].bone_count = 0;

        m_instance_slots_staging.push_back(slot);
        ++debug_idx;
    }
    current_frame.buffers.objects.end();

    m_debug_light_instance_base =
        static_cast<uint32_t>(m_instance_slots_staging.size()) - light_count;
    m_debug_light_draw_count = light_count;

    // Re-upload instance slots with debug entries appended
    upload_instance_slots(current_frame);
}

void
vulkan_render::draw_debug_lights(VkCommandBuffer cmd, render::frame_state& current_frame)
{
    ZoneScopedN("Render::DrawDebugLights");

    if (!m_render_config.debug.light_wireframe)
    {
        return;
    }

    if (!m_debug_wire_se || !m_debug_wire_mat || m_debug_wire_se->m_failed_load)
    {
        return;
    }

    if (m_debug_light_draw_count == 0)
    {
        return;
    }

    if (!m_debug_sphere_mesh || !m_debug_cone_mesh)
    {
        return;
    }

    // Bind debug wire pipeline
    auto* se = m_debug_wire_se;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, se->m_pipeline);

    auto pipeline_layout = se->m_pipeline_layout;

    if (m_bindless_set != VK_NULL_HANDLE)
    {
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout,
                                KGPU_textures_descriptor_sets,
                                1,
                                &m_bindless_set,
                                0,
                                nullptr);
    }

    gpu::push_constants_main pc = m_obj_config;
    pc.material_id = m_debug_wire_mat->gpu_idx();
    pc.use_clustered_lighting = 0;
    pc.directional_light_id = 0;
    for (uint32_t t = 0; t < KGPU_MAX_TEXTURE_SLOTS; ++t)
    {
        pc.texture_indices[t] = UINT32_MAX;
        pc.sampler_indices[t] = KGPU_SAMPLER_LINEAR_REPEAT;
    }

    // Draw each light with the appropriate mesh (sphere for point, cone for spot)
    uint32_t draw_idx = 0;
    for (uint32_t i = 0; i < m_loader->uni_lights_size(); ++i)
    {
        auto* light = m_loader->uni_light_at(i);
        if (!light || !light->is_valid())
        {
            continue;
        }

        bool is_spot = light->gpu_data.cut_off > 0.0f;
        auto* mesh = is_spot ? m_debug_cone_mesh : m_debug_sphere_mesh;

        bind_mesh(cmd, mesh);

        pc.instance_base = m_debug_light_instance_base + draw_idx;

        vkCmdPushConstants(cmd,
                           se->m_pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(gpu::push_constants_main),
                           &pc);

        if (mesh->has_indices())
        {
            vkCmdDrawIndexed(cmd, mesh->indices_size(), 1, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(cmd, mesh->vertices_size(), 1, 0, 0);
        }

        ++draw_idx;
    }
}

}  // namespace render
}  // namespace kryga
