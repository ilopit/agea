#include "vulkan_render/kryga_render.h"

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/types/vulkan_mesh_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"

#include <gpu_types/gpu_generic_constants.h>
#include <gpu_types/gpu_shadow_types.h>

#include <utils/kryga_log.h>

#include <global_state/global_state.h>

#include <tracy/Tracy.hpp>

#include <cmath>
#include <algorithm>

namespace kryga
{
namespace render
{

// ============================================================================
// Cascade Split Computation (PSSM)
// ============================================================================

void
vulkan_render::compute_cascade_splits(float near, float far, float lambda)
{
    for (uint32_t i = 0; i < KGPU_CSM_CASCADE_COUNT; ++i)
    {
        float p = static_cast<float>(i + 1) / static_cast<float>(KGPU_CSM_CASCADE_COUNT);

        // Logarithmic split
        float log_split = near * std::pow(far / near, p);

        // Uniform split
        float uniform_split = near + (far - near) * p;

        // Blend between logarithmic and uniform
        float split = lambda * log_split + (1.0f - lambda) * uniform_split;

        m_shadow_config.directional.cascades[i].split_depth = split;
    }
}

// ============================================================================
// Shadow Matrix Computation
// ============================================================================

void
vulkan_render::compute_shadow_matrices()
{
    ZoneScopedN("Render::ComputeShadowMatrices");

    // Get directional light direction
    glm::vec3 light_dir(0.0f, -1.0f, 0.0f);
    if (m_cache.directional_lights.get_actual_size() > 0)
    {
        auto slot = get_selected_directional_light_slot();
        auto* dl = m_cache.directional_lights.at(slot);
        if (dl)
        {
            light_dir = glm::normalize(glm::vec3(
                dl->gpu_data.direction.x, dl->gpu_data.direction.y, dl->gpu_data.direction.z));
        }
    }

    float near_clip = KGPU_znear;
    float far_clip = KGPU_zfar;
    float shadow_far = std::min(far_clip, m_render_config.shadows.distance);

    compute_cascade_splits(near_clip, shadow_far, 0.5f);

    // Single light view shared by all cascades.
    // Center on camera position projected onto the ground (Y=0) so shadows follow the player.
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(light_dir, up)) > 0.99f)
    {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    // Fixed shadow center at origin. The largest cascade covers ~242 units from center
    // (484m total), which is more than enough for typical scenes.
    // A camera-following approach requires shadow map scrolling to avoid re-render artifacts.
    glm::vec3 shadow_center(0.0f);
    glm::vec3 eye = shadow_center - light_dir * 500.0f;
    glm::mat4 light_view = glm::lookAt(eye, shadow_center, up);

    // Extract FOV and aspect from the camera projection matrix
    // For perspective with GLM_FORCE_DEPTH_ZERO_TO_ONE: proj[1][1] = 1/tan(fov/2)
    float tan_half_fov = 1.0f / m_camera_data.projection[1][1];
    float aspect = m_camera_data.projection[1][1] / m_camera_data.projection[0][0];

    float last_split = near_clip;

    for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
    {
        float split_far = m_shadow_config.directional.cascades[c].split_depth;
        float split_near = last_split;
        last_split = split_far;

        // Radius from far-plane diagonal of this cascade slice
        float hh = split_far * tan_half_fov;
        float hw = hh * aspect;
        float depth_half = (split_far - split_near) * 0.5f;
        float radius = std::sqrt(hw * hw + hh * hh + depth_half * depth_half);
        radius = std::ceil(radius);
        if (radius < 1.0f)
        {
            radius = 1.0f;
        }

        // Tight Z range: eye is 500 units behind shadow_center along light direction.
        // Scene objects are within ~radius units of shadow_center.
        // In view space, objects are at z ≈ -(500 ± radius).
        // ortho near/far measure distance along -Z from eye.
        float z_eye_dist = 500.0f;
        float ortho_near = z_eye_dist - radius - 50.0f;
        float ortho_far = z_eye_dist + radius + 50.0f;
        if (ortho_near < 0.1f)
        {
            ortho_near = 0.1f;
        }

        glm::mat4 light_proj = glm::ortho(-radius, radius, -radius, radius, ortho_near, ortho_far);

        // Texel snapping on the final VP
        glm::mat4 shadow_vp = light_proj * light_view;
        float half_sm = (float)m_render_config.shadows.map_size * 0.5f;
        glm::vec4 origin = shadow_vp * glm::vec4(0, 0, 0, 1);
        float sx = std::round(origin.x * half_sm) - origin.x * half_sm;
        float sy = std::round(origin.y * half_sm) - origin.y * half_sm;
        light_proj[3][0] += sx / half_sm;
        light_proj[3][1] += sy / half_sm;

        m_shadow_config.directional.cascades[c].view_proj = light_proj * light_view;
    }
}

// ============================================================================
// Shadow Pass Drawing
// ============================================================================

void
vulkan_render::draw_shadow_pass(VkCommandBuffer cmd, uint32_t cascade_idx)
{
    ZoneScopedN("Render::DrawShadowPass");

    if (!m_render_config.shadows.enabled || !m_shadow_se || m_draw_batches.empty())
    {
        return;
    }

    auto* se = m_shadow_se;
    if (se->m_failed_load)
    {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, se->m_pipeline);

    // Set 1: objects, instance_slots, shadow_data — built once per frame.
    if (m_shadow_objects_set != VK_NULL_HANDLE)
    {
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                se->m_pipeline_layout,
                                KGPU_objects_descriptor_sets,
                                1,
                                &m_shadow_objects_set,
                                0,
                                nullptr);
    }

    // Shadow push constants with cascade index
    gpu::push_constants_shadow pc = m_shadow_pc;
    pc.instance_base = 0;
    pc.directional_light_id = cascade_idx;
    pc.use_clustered_lighting = 0;  // 0 = CSM cascade mode

    // Draw shadow-casting opaque batches into shadow map
    for (const auto& batch : m_draw_batches)
    {
        if (!batch.mesh || !batch.cast_shadows)
        {
            continue;
        }

        // Safe mesh binding — copy handle to local to avoid null-handle Vulkan errors
        VkBuffer vb = batch.mesh->m_vertex_buffer.buffer();
        if (!vb)
        {
            continue;
        }

        VkDeviceSize vb_offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &vb_offset);

        bool indexed = batch.mesh->has_indices();
        if (indexed)
        {
            VkBuffer ib = batch.mesh->m_index_buffer.buffer();
            if (!ib)
            {
                continue;
            }
            vkCmdBindIndexBuffer(cmd, ib, 0, VK_INDEX_TYPE_UINT32);
        }

        pc.instance_base = batch.first_instance_offset;
        vkCmdPushConstants(cmd,
                           se->m_pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(gpu::push_constants_shadow),
                           &pc);

        if (indexed)
        {
            vkCmdDrawIndexed(cmd, batch.mesh->indices_size(), batch.instance_count, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(cmd, batch.mesh->vertices_size(), batch.instance_count, 0, 0);
        }
    }
}

// ============================================================================
// Local Light Shadow Drawing
// ============================================================================

void
vulkan_render::draw_shadow_local_pass(VkCommandBuffer cmd, uint32_t shadow_idx, bool back_face)
{
    ZoneScopedN("Render::DrawShadowLocalPass");

    if (!m_render_config.shadows.enabled)
    {
        return;
    }

    // Skip if this shadow slot is unused this frame
    if (shadow_idx >= m_shadow_config.shadowed_local_count)
    {
        return;
    }

    // Spot lights only need front face — skip back hemisphere
    bool is_point =
        m_shadow_config.local_shadows[shadow_idx].shadow_info.z == KGPU_light_type_point;
    if (back_face && !is_point)
    {
        return;
    }

    if (m_draw_batches.empty())
    {
        return;
    }

    auto* se = is_point ? m_shadow_dpsm_se : m_shadow_se;

    if (!se || se->m_failed_load)
    {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, se->m_pipeline);

    if (m_shadow_objects_set != VK_NULL_HANDLE)
    {
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                se->m_pipeline_layout,
                                KGPU_objects_descriptor_sets,
                                1,
                                &m_shadow_objects_set,
                                0,
                                nullptr);
    }

    // Shadow push constants
    gpu::push_constants_shadow pc = m_shadow_pc;
    pc.directional_light_id = shadow_idx;
    // For spot lights (se_shadow.vert): use_clustered_lighting=1 means "local light mode"
    // For point lights (se_shadow_dpsm.vert): use_clustered_lighting encodes hemisphere (0=front,
    // 1=back)
    if (is_point)
    {
        pc.use_clustered_lighting = back_face ? 1u : 0u;
    }
    else
    {
        pc.use_clustered_lighting = 1u;
    }

    for (const auto& batch : m_draw_batches)
    {
        if (!batch.mesh || !batch.cast_shadows)
        {
            continue;
        }

        VkBuffer vb = batch.mesh->m_vertex_buffer.buffer();
        if (!vb)
        {
            continue;
        }

        VkDeviceSize vb_offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &vb_offset);

        bool indexed = batch.mesh->has_indices();
        if (indexed)
        {
            VkBuffer ib = batch.mesh->m_index_buffer.buffer();
            if (!ib)
            {
                continue;
            }
            vkCmdBindIndexBuffer(cmd, ib, 0, VK_INDEX_TYPE_UINT32);
        }

        pc.instance_base = batch.first_instance_offset;
        vkCmdPushConstants(cmd,
                           se->m_pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(gpu::push_constants_shadow),
                           &pc);

        if (indexed)
        {
            vkCmdDrawIndexed(cmd, batch.mesh->indices_size(), batch.instance_count, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(cmd, batch.mesh->vertices_size(), batch.instance_count, 0, 0);
        }
    }
}

// ============================================================================
// Shadow Light Selection
// ============================================================================

void
vulkan_render::select_shadowed_lights()
{
    ZoneScopedN("Render::SelectShadowedLights");

    m_shadow_config.shadowed_local_count = 0;

    // Collect candidate lights with their screen-space contribution
    struct shadow_candidate
    {
        uint32_t light_slot;
        float contribution;
    };
    std::vector<shadow_candidate> candidates;

    for (uint32_t i = 0; i < m_cache.universal_lights.get_size(); ++i)
    {
        auto* light = m_cache.universal_lights.at(i);
        if (!light->is_valid())
        {
            continue;
        }

        float dist = glm::length(light->gpu_data.position - m_camera_data.position);
        if (dist > light->gpu_data.radius * 3.0f)
        {
            continue;  // Too far to matter
        }

        float contribution = light->gpu_data.radius / std::max(dist, 0.1f);
        candidates.push_back({light->slot(), contribution});
    }

    // Sort by contribution (highest first)
    std::sort(candidates.begin(),
              candidates.end(),
              [](const auto& a, const auto& b) { return a.contribution > b.contribution; });

    // Take top N lights
    uint32_t count =
        std::min((uint32_t)candidates.size(), (uint32_t)KGPU_MAX_SHADOWED_LOCAL_LIGHTS);
    m_shadow_config.shadowed_local_count = count;

    auto frame_idx = glob::glob_state().getr_render_device().get_current_frame_index();

    for (uint32_t i = 0; i < count; ++i)
    {
        auto* light = m_cache.universal_lights.at(candidates[i].light_slot);

        auto& shadow = m_shadow_config.local_shadows[i];
        shadow.shadow_info.z = light->gpu_data.type;  // light_type
        float s_near = 0.1f;
        float s_far = light->gpu_data.radius;
        shadow.shadow_params =
            glm::vec4(0.005f,                                                       // bias
                      0.02f,                                                        // normal_bias
                      1.0f / static_cast<float>(m_render_config.shadows.map_size),  // texel_size
                      s_near                                                        // near_plane
            );
        shadow.far_plane = s_far;

        if (light->gpu_data.type == KGPU_light_type_spot)
        {
            // Spot light: perspective projection
            float fov = 2.0f * std::acos(light->gpu_data.outer_cut_off);
            glm::mat4 proj = glm::perspective(fov, 1.0f, s_near, s_far);
            glm::mat4 view =
                glm::lookAt(light->gpu_data.position,
                            light->gpu_data.position + glm::normalize(light->gpu_data.direction),
                            glm::vec3(0, 1, 0));

            if (std::abs(glm::dot(glm::normalize(light->gpu_data.direction), glm::vec3(0, 1, 0))) >
                0.99f)
            {
                view = glm::lookAt(
                    light->gpu_data.position,
                    light->gpu_data.position + glm::normalize(light->gpu_data.direction),
                    glm::vec3(0, 0, 1));
            }

            shadow.view_proj = proj * view;
            shadow.view_proj_back = glm::mat4(0.0f);
            shadow.shadow_info.x = m_shadow_local_bindless_indices[i * 2][frame_idx];
            shadow.shadow_info.y = 0xFFFFFFFFu;
        }
        else
        {
            // Point light: dual-paraboloid
            glm::vec3 front_dir(0.0f, 0.0f, 1.0f);
            glm::mat4 view = glm::lookAt(
                light->gpu_data.position, light->gpu_data.position + front_dir, glm::vec3(0, 1, 0));
            shadow.view_proj = view;
            shadow.view_proj_back = view;
            shadow.shadow_info.x = m_shadow_local_bindless_indices[i * 2][frame_idx];
            shadow.shadow_info.y = m_shadow_local_bindless_indices[i * 2 + 1][frame_idx];
        }

        // Set shadow_index on the light
        light->gpu_data.shadow_index = i;
    }

    // Clear shadow_index on lights that didn't make the cut
    for (uint32_t i = 0; i < m_cache.universal_lights.get_size(); ++i)
    {
        auto* light = m_cache.universal_lights.at(i);
        if (!light->is_valid())
        {
            continue;
        }

        bool found = false;
        for (uint32_t j = 0; j < count; ++j)
        {
            if (candidates[j].light_slot == light->slot())
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            light->gpu_data.shadow_index = KGPU_SHADOW_INDEX_NONE;
        }
    }
}

// ============================================================================
// Shadow Data Upload
// ============================================================================

void
vulkan_render::upload_shadow_data(render::frame_state& frame)
{
    ZoneScopedN("Render::UploadShadowData");

    // Update CSM bindless indices for the current frame-in-flight
    auto frame_idx = glob::glob_state().getr_render_device().get_current_frame_index();
    for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
    {
        m_shadow_config.directional.shadow_map_indices[c] =
            m_shadow_map_bindless_indices[c][frame_idx];
    }

    // Compute shadow matrices for this frame
    compute_shadow_matrices();

    // Select which local lights get shadows (modifies gpu_data.shadow_index on lights)
    select_shadowed_lights();

    // Re-upload universal light data so shadow_index is in the SSBO
    if (m_cache.universal_lights.get_size() > 0)
    {
        const auto total_size =
            m_cache.universal_lights.get_size() * sizeof(gpu::universal_light_data);
        if (total_size <= frame.buffers.universal_lights.get_alloc_size())
        {
            frame.buffers.universal_lights.begin();
            auto* dst = reinterpret_cast<gpu::universal_light_data*>(
                frame.buffers.universal_lights.allocate_data(static_cast<uint32_t>(total_size)));
            for (uint32_t i = 0; i < m_cache.universal_lights.get_size(); ++i)
            {
                auto* light = m_cache.universal_lights.at(i);
                dst[i] = light->gpu_data;
            }
            frame.buffers.universal_lights.end();
        }
    }

    // Upload shadow config to GPU
    frame.buffers.shadow_data.begin();
    auto* data = frame.buffers.shadow_data.allocate_data(sizeof(gpu::shadow_config_data));
    memcpy(data, &m_shadow_config, sizeof(gpu::shadow_config_data));
    frame.buffers.shadow_data.end();
}

}  // namespace render
}  // namespace kryga
