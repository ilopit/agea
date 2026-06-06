#include "vulkan_render/kryga_render.h"
#include "vulkan_render/render_thread.h"

#include "vulkan_render/render_system.h"
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
// Shadow Atlas Layout
// ============================================================================

void
vulkan_render::compute_shadow_atlas_layout()
{
    const uint32_t atlas_sz = m_render_config.shadows.atlas_size;
    const uint32_t csm_sz = m_render_config.shadows.csm_tile_size;
    const uint32_t local_sz = m_render_config.shadows.local_tile_size;
    const float inv_atlas = 1.0f / float(atlas_sz);

    for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
    {
        auto& t = m_csm_tiles[c];
        t.x = c * csm_sz;
        t.y = 0;
        t.size = csm_sz;
        t.uv_offset = glm::vec2(float(t.x) * inv_atlas, float(t.y) * inv_atlas);
        t.uv_scale = glm::vec2(float(csm_sz) * inv_atlas);
    }

    const uint32_t cols = atlas_sz / local_sz;
    const uint32_t local_tile_count = m_render_config.shadows.max_local_lights * 2;
    for (uint32_t i = 0; i < local_tile_count; ++i)
    {
        auto& t = m_local_tiles[i];
        uint32_t col = i % cols;
        uint32_t row = i / cols;
        t.x = col * local_sz;
        t.y = csm_sz + row * local_sz;
        t.size = local_sz;
        t.uv_offset = glm::vec2(float(t.x) * inv_atlas, float(t.y) * inv_atlas);
        t.uv_scale = glm::vec2(float(local_sz) * inv_atlas);
    }
}

// ============================================================================
// Shadow Atlas Drawing
// ============================================================================

void
vulkan_render::draw_shadow_atlas(VkCommandBuffer cmd)
{
    if (!m_render_config.shadows.enabled)
    {
        return;
    }
    KRG_check(m_shadow_se, "shadow shader effect must exist when shadows are enabled");

    // CSM cascades
    for (uint32_t c = 0; c < m_render_config.shadows.cascade_count; ++c)
    {
        auto& tile = m_csm_tiles[c];

        VkViewport vp{};
        vp.x = static_cast<float>(tile.x);
        vp.y = static_cast<float>(tile.y);
        vp.width = static_cast<float>(tile.size);
        vp.height = static_cast<float>(tile.size);
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D sc{};
        sc.offset = {static_cast<int32_t>(tile.x), static_cast<int32_t>(tile.y)};
        sc.extent = {tile.size, tile.size};
        vkCmdSetScissor(cmd, 0, 1, &sc);

        draw_shadow_pass(cmd, c);
    }

    // Local light shadows
    for (uint32_t i = 0; i < m_shadow_config.shadowed_local_count; ++i)
    {
        bool is_point = m_shadow_config.local_shadows[i].shadow_info.z == KGPU_light_type_point;

        // Front hemisphere
        {
            auto& tile = m_local_tiles[i * 2];
            VkViewport vp{};
            vp.x = static_cast<float>(tile.x);
            vp.y = static_cast<float>(tile.y);
            vp.width = static_cast<float>(tile.size);
            vp.height = static_cast<float>(tile.size);
            vp.minDepth = 0.0f;
            vp.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &vp);

            VkRect2D sc{};
            sc.offset = {static_cast<int32_t>(tile.x), static_cast<int32_t>(tile.y)};
            sc.extent = {tile.size, tile.size};
            vkCmdSetScissor(cmd, 0, 1, &sc);

            draw_shadow_local_pass(cmd, i, false);
        }

        // Back hemisphere (point lights only)
        if (is_point)
        {
            auto& tile = m_local_tiles[i * 2 + 1];
            VkViewport vp{};
            vp.x = static_cast<float>(tile.x);
            vp.y = static_cast<float>(tile.y);
            vp.width = static_cast<float>(tile.size);
            vp.height = static_cast<float>(tile.size);
            vp.minDepth = 0.0f;
            vp.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &vp);

            VkRect2D sc{};
            sc.offset = {static_cast<int32_t>(tile.x), static_cast<int32_t>(tile.y)};
            sc.extent = {tile.size, tile.size};
            vkCmdSetScissor(cmd, 0, 1, &sc);

            draw_shadow_local_pass(cmd, i, true);
        }
    }
}

// ============================================================================
// Cascade Split Computation (PSSM)
// ============================================================================

void
vulkan_render::compute_cascade_splits(float near, float far, float lambda)
{
    uint32_t count = m_render_config.shadows.cascade_count;
    for (uint32_t i = 0; i < count; ++i)
    {
        float p = static_cast<float>(i + 1) / static_cast<float>(count);

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

    compute_cascade_splits(near_clip, shadow_far, m_render_config.shadows.cascade_split_lambda);

    glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(light_dir, up)) > 0.99f)
    {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    // Camera-following CSM: each cascade is centered on its own view-frustum slice so
    // shadows track the viewer anywhere in the world (the old code pinned every cascade
    // to the origin, so shadows vanished past ~242m and near cascades drifted off the
    // camera). camera looks down -Z, so inv_view[2] is the camera's +Z in world.
    glm::vec3 cam_pos = m_camera_data.position;
    glm::mat4 inv_view = glm::inverse(m_camera_data.view);
    glm::vec3 cam_fwd = -glm::normalize(glm::vec3(inv_view[2]));

    // Distance from cascade center to eye along the light direction. Generous (and
    // constant per frame) so casters behind the slice along the light axis still make it
    // into the depth map; kept at the previous value to preserve caster capture.
    const float pullback = 500.0f;

    // Extract FOV and aspect from the camera projection matrix
    // For perspective with GLM_FORCE_DEPTH_ZERO_TO_ONE: proj[1][1] = 1/tan(fov/2)
    float tan_half_fov = 1.0f / m_camera_data.projection[1][1];
    float aspect = m_camera_data.projection[1][1] / m_camera_data.projection[0][0];

    float last_split = near_clip;

    for (uint32_t c = 0; c < m_render_config.shadows.cascade_count; ++c)
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

        // Cascade center = slice axial midpoint along the camera forward axis. The radius
        // above is the distance from this midpoint to a far corner, which also bounds the
        // (smaller) near corners, so midpoint + radius is a valid enclosing sphere.
        float center_dist = (split_near + split_far) * 0.5f;
        glm::vec3 center = cam_pos + cam_fwd * center_dist;

        // Per-cascade light view centered on the moving slice center.
        glm::vec3 eye = center - light_dir * pullback;
        glm::mat4 light_view = glm::lookAt(eye, center, up);

        // Z range measures distance along -Z from eye; objects are within ~radius of
        // center, i.e. at z ≈ -(pullback ± radius).
        float ortho_near = pullback - radius - 50.0f;
        float ortho_far = pullback + radius + 50.0f;
        if (ortho_near < 0.1f)
        {
            ortho_near = 0.1f;
        }

        glm::mat4 light_proj = glm::ortho(-radius, radius, -radius, radius, ortho_near, ortho_far);

        // Texel snap against a FIXED world reference (the origin), NOT `center`.
        // light_view = lookAt(eye, center) puts `center` at light-space (0,0) every frame,
        // so snapping the center is a no-op (round(0)=0) and the grid crawls => jitter.
        // Projecting a fixed world point and quantizing the box translation so it lands on
        // a texel boundary locks the texel grid to world space (texel size is constant
        // frame-to-frame because radius is camera-rotation independent), so the grid only
        // ever shifts in whole-texel steps as the box moves => no shimmer.
        glm::mat4 shadow_vp = light_proj * light_view;
        float half_sm = (float)m_render_config.shadows.csm_tile_size * 0.5f;
        glm::vec4 ref_ndc = shadow_vp * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        float sx = std::round(ref_ndc.x * half_sm) - ref_ndc.x * half_sm;
        float sy = std::round(ref_ndc.y * half_sm) - ref_ndc.y * half_sm;
        light_proj[3][0] += sx / half_sm;
        light_proj[3][1] += sy / half_sm;

        m_shadow_config.directional.cascades[c].view_proj = light_proj * light_view;
        m_shadow_config.directional.cascades[c].texel_world_size =
            (2.0f * radius) / static_cast<float>(m_render_config.shadows.csm_tile_size);
    }
}

// ============================================================================
// Shadow Pass Drawing
// ============================================================================

void
vulkan_render::draw_shadow_pass(VkCommandBuffer cmd, uint32_t cascade_idx)
{
    ZoneScopedN("Render::DrawShadowPass");

    if (!m_render_config.shadows.enabled)
    {
        return;
    }
    KRG_check(m_shadow_se, "shadow shader effect must exist when shadows are enabled");

    // Per-cascade culled caster list (#7) — nothing visible to this cascade, skip.
    const auto& batches = m_cascade_shadow_batches[cascade_idx];
    if (batches.empty())
    {
        return;
    }

    auto* se = m_shadow_se;
    KRG_check(!se->m_failed_load, "shadow shader effect failed to load");

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, se->m_pipeline);

    // Shadow push constants with cascade index
    gpu::push_constants_shadow pc = m_shadow_pc;
    pc.instance_base = 0;
    pc.directional_light_id = cascade_idx;
    pc.use_clustered_lighting = 0;  // 0 = CSM cascade mode

    // Draw shadow-casting opaque batches into shadow map
    for (const auto& batch : batches)
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

    // Per-light culled caster list (#7): index = light*2 + hemisphere.
    const auto& batches = m_local_shadow_batches[shadow_idx * 2 + (back_face ? 1 : 0)];
    if (batches.empty())
    {
        return;
    }

    auto* se = is_point ? m_shadow_dpsm_se : m_shadow_se;

    if (!se || se->m_failed_load)
    {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, se->m_pipeline);

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

    for (const auto& batch : batches)
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
        float contribution = light->gpu_data.radius / std::max(dist, 0.1f);
        candidates.push_back({light->slot(), contribution});
    }

    // Sort by contribution (highest first)
    std::sort(candidates.begin(),
              candidates.end(),
              [](const auto& a, const auto& b) { return a.contribution > b.contribution; });

    // Take top N lights
    uint32_t count =
        std::min((uint32_t)candidates.size(), m_render_config.shadows.max_local_lights);
    m_shadow_config.shadowed_local_count = count;

    for (uint32_t i = 0; i < count; ++i)
    {
        auto* light = m_cache.universal_lights.at(candidates[i].light_slot);

        auto& shadow = m_shadow_config.local_shadows[i];
        shadow.shadow_info.z = light->gpu_data.type;
        float s_near = 0.1f;
        float s_far = light->gpu_data.radius;
        // x = depth bias, y = normal bias — sampled by calcSpotShadow/calcPointShadow.
        // Driven by the dedicated shadows.local_bias / shadows.local_normal_bias knobs,
        // independent of the directional shadows.bias / shadows.normal_bias.
        shadow.shadow_params =
            glm::vec4(m_render_config.shadows.local_bias,
                      m_render_config.shadows.local_normal_bias,
                      1.0f / static_cast<float>(m_render_config.shadows.local_tile_size),
                      s_near);
        shadow.far_plane = s_far;

        // Atlas UV for front and back tiles
        shadow.atlas_offset_front = m_local_tiles[i * 2].uv_offset;
        shadow.atlas_scale_front = m_local_tiles[i * 2].uv_scale;
        shadow.atlas_offset_back = m_local_tiles[i * 2 + 1].uv_offset;
        shadow.atlas_scale_back = m_local_tiles[i * 2 + 1].uv_scale;

        if (light->gpu_data.type == KGPU_light_type_spot)
        {
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
        }
        else
        {
            glm::vec3 front_dir(0.0f, 0.0f, 1.0f);
            glm::mat4 view = glm::lookAt(
                light->gpu_data.position, light->gpu_data.position + front_dir, glm::vec3(0, 1, 0));
            shadow.view_proj = view;
            shadow.view_proj_back = view;
        }

        // Stash CPU-side cull data consumed by prepare_instance_data (#7). front_dir
        // for points matches the hardcoded (0,0,1) used to build the point view above.
        auto& cull = m_local_shadow_cull[i];
        cull.position = light->gpu_data.position;
        cull.radius = s_far;
        cull.type = light->gpu_data.type;
        cull.front_dir = (light->gpu_data.type == KGPU_light_type_point)
                             ? glm::vec3(0.0f, 0.0f, 1.0f)
                             : glm::normalize(light->gpu_data.direction);

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
    KRG_check_render_thread();
    ZoneScopedN("Render::UploadShadowData");

    m_shadow_config.atlas_bindless_index = m_shadow_atlas_bindless_index;

    // Set per-cascade atlas UV data
    for (uint32_t c = 0; c < m_render_config.shadows.cascade_count; ++c)
    {
        m_shadow_config.directional.cascades[c].atlas_offset = m_csm_tiles[c].uv_offset;
        m_shadow_config.directional.cascades[c].atlas_scale = m_csm_tiles[c].uv_scale;
    }

    // compute_shadow_matrices() and select_shadowed_lights() now run earlier, in
    // prepare_draw_resources before prepare_instance_data, so the cascade/local
    // frustums exist when shadow caster batches are culled per pass (#7).

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
