#include "vulkan_render/kryga_render.h"

#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/types/vulkan_render_pass_builder.h"
#include "vulkan_render/types/vulkan_material_data.h"
#include "vulkan_render/types/vulkan_mesh_data.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/vulkan_render_loader_create_infos.h"
#include "vulkan_render/utils/vulkan_initializers.h"

#include <gpu_types/gpu_generic_constants.h>
#include <gpu_types/gpu_shadow_types.h>

#include <utils/kryga_log.h>
#include <utils/buffer.h>

#include <resource_locator/resource_locator.h>
#include <global_state/global_state.h>

#include <tracy/Tracy.hpp>

#include <cmath>
#include <algorithm>

namespace kryga
{
namespace render
{

// ============================================================================
// Shadow Pass Initialization
// ============================================================================

void
vulkan_render::init_shadow_passes()
{
    ZoneScopedN("Render::InitShadowPasses");

    // Create 4 depth-only render passes for CSM cascades
    for (uint32_t c = 0; c < KGPU_CSM_CASCADE_COUNT; ++c)
    {
        m_shadow_passes[c] = render_pass_builder()
                                 .set_depth_format(VK_FORMAT_D32_SFLOAT)
                                 .set_depth_only(true)
                                 .set_image_count(1)
                                 .set_width_depth(KGPU_SHADOW_MAP_SIZE, KGPU_SHADOW_MAP_SIZE)
                                 .set_enable_stencil(false)
                                 .build();

        m_shadow_passes[c]->set_name(AID("shadow_csm_" + std::to_string(c)));

        // Register depth image view in bindless texture array
        auto depth_view = m_shadow_passes[c]->get_depth_image_view(0);

        // Use reserved high indices for shadow maps to avoid conflicts with texture slots
        auto& device = glob::glob_state().getr_render_device();
        uint32_t bindless_idx = KGPU_max_bindless_textures - 1 - c;

        VkDescriptorImageInfo image_info = {};
        image_info.imageView = depth_view;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_bindless_set;
        write.dstBinding = 1;  // bindless_textures binding
        write.dstArrayElement = bindless_idx;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo = &image_info;

        vkUpdateDescriptorSets(device.vk_device(), 1, &write, 0, nullptr);

        m_shadow_map_bindless_indices[c] = bindless_idx;
        m_shadow_config.directional.shadow_map_indices[c] = bindless_idx;  // uvec4 component
    }

    // Create shadow vertex shader effect on the first shadow pass.
    // Use the pick shader's pipeline layout so descriptor sets are compatible.
    VkPipelineLayout shared_layout = VK_NULL_HANDLE;
    if (m_pick_mat && m_pick_mat->get_shader_effect())
    {
        shared_layout = m_pick_mat->get_shader_effect()->m_pipeline_layout;
    }

    auto path = glob::glob_state().get_resource_locator()->resource(
        category::packages, "base.apkg/class/shader_effects");

    kryga::utils::buffer vert;
    auto vert_path = path / "shadow/se_shadow.vert";
    if (kryga::utils::buffer::load(vert_path, vert))
    {
        shader_effect_create_info se_ci = {};
        se_ci.vert_buffer = &vert;
        se_ci.frag_buffer = nullptr;  // No fragment shader for depth-only
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = false;
        se_ci.alpha = alpha_mode::none;
        se_ci.cull_mode = VK_CULL_MODE_FRONT_BIT;  // Front-face culling reduces peter-panning
        se_ci.height = KGPU_SHADOW_MAP_SIZE;
        se_ci.width = KGPU_SHADOW_MAP_SIZE;
        se_ci.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
        se_ci.shared_pipeline_layout = shared_layout;

        m_shadow_se = nullptr;
        auto rc = m_shadow_passes[0]->create_shader_effect(AID("se_shadow"), se_ci, m_shadow_se);
        if (rc != result_code::ok)
        {
            ALOG_WARN("Failed to create shadow shader effect - shadows disabled");
            m_shadow_se = nullptr;
        }
    }
    else
    {
        ALOG_WARN("Failed to load se_shadow.vert - shadows disabled");
    }

    // Create local light shadow passes (spot + point DPSM)
    // Each local light needs up to 2 passes (point lights need front+back)
    for (uint32_t i = 0; i < KGPU_MAX_SHADOWED_LOCAL_LIGHTS * 2; ++i)
    {
        m_shadow_local_passes[i] = render_pass_builder()
                                       .set_depth_format(VK_FORMAT_D32_SFLOAT)
                                       .set_depth_only(true)
                                       .set_image_count(1)
                                       .set_width_depth(KGPU_SHADOW_MAP_SIZE, KGPU_SHADOW_MAP_SIZE)
                                       .set_enable_stencil(false)
                                       .build();

        m_shadow_local_passes[i]->set_name(AID("shadow_local_" + std::to_string(i)));

        // Register in bindless array
        auto local_depth_view = m_shadow_local_passes[i]->get_depth_image_view(0);
        uint32_t local_bindless_idx = KGPU_max_bindless_textures - 1 - KGPU_CSM_CASCADE_COUNT - i;

        VkDescriptorImageInfo local_image_info = {};
        local_image_info.imageView = local_depth_view;
        local_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet local_write = {};
        local_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        local_write.dstSet = m_bindless_set;
        local_write.dstBinding = 1;
        local_write.dstArrayElement = local_bindless_idx;
        local_write.descriptorCount = 1;
        local_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        local_write.pImageInfo = &local_image_info;

        auto vk_dev = glob::glob_state().getr_render_device().vk_device();
        vkUpdateDescriptorSets(vk_dev, 1, &local_write, 0, nullptr);

        m_shadow_local_bindless_indices[i] = local_bindless_idx;
    }

    // Create DPSM vertex shader for point lights
    auto dpsm_vert_path = path / "shadow/se_shadow_dpsm.vert";
    kryga::utils::buffer dpsm_vert;
    if (kryga::utils::buffer::load(dpsm_vert_path, dpsm_vert))
    {
        shader_effect_create_info se_ci = {};
        se_ci.vert_buffer = &dpsm_vert;
        se_ci.frag_buffer = nullptr;
        se_ci.is_wire = false;
        se_ci.enable_dynamic_state = false;
        se_ci.alpha = alpha_mode::none;
        se_ci.cull_mode = VK_CULL_MODE_NONE;  // Can't cull in paraboloid space
        se_ci.height = KGPU_SHADOW_MAP_SIZE;
        se_ci.width = KGPU_SHADOW_MAP_SIZE;
        se_ci.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
        se_ci.shared_pipeline_layout = shared_layout;

        m_shadow_dpsm_se = nullptr;
        auto rc = m_shadow_passes[0]->create_shader_effect(AID("se_shadow_dpsm"), se_ci,
                                                           m_shadow_dpsm_se);
        if (rc != result_code::ok)
        {
            ALOG_WARN("Failed to create DPSM shadow shader effect");
            m_shadow_dpsm_se = nullptr;
        }
    }

    // Initialize shadow config defaults
    m_shadow_config.directional.cascade_count = KGPU_CSM_CASCADE_COUNT;
    m_shadow_config.directional.shadow_bias = 0.005f;
    m_shadow_config.directional.normal_bias = 0.03f;
    m_shadow_config.directional.texel_size = 1.0f / static_cast<float>(KGPU_SHADOW_MAP_SIZE);
    m_shadow_config.directional.pcf_mode = KGPU_PCF_POISSON16;
    m_shadow_config.shadowed_local_count = 0;

    ALOG_INFO("Shadow passes initialized: {} CSM cascades, {}x{} resolution",
              KGPU_CSM_CASCADE_COUNT, KGPU_SHADOW_MAP_SIZE, KGPU_SHADOW_MAP_SIZE);
}

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
            light_dir = glm::normalize(glm::vec3(dl->gpu_data.direction.x, dl->gpu_data.direction.y,
                                                 dl->gpu_data.direction.z));
        }
    }

    float near_clip = KGPU_znear;
    float far_clip = KGPU_zfar;
    float shadow_far = std::min(far_clip, m_shadow_distance);

    compute_cascade_splits(near_clip, shadow_far, 0.5f);

    // Single light view shared by all cascades.
    // Center on camera position projected onto the ground (Y=0) so shadows follow the player.
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(light_dir, up)) > 0.99f)
        up = glm::vec3(0.0f, 0.0f, 1.0f);

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
            radius = 1.0f;

        // Tight Z range: eye is 500 units behind shadow_center along light direction.
        // Scene objects are within ~radius units of shadow_center.
        // In view space, objects are at z ≈ -(500 ± radius).
        // ortho near/far measure distance along -Z from eye.
        float z_eye_dist = 500.0f;
        float ortho_near = z_eye_dist - radius - 50.0f;
        float ortho_far = z_eye_dist + radius + 50.0f;
        if (ortho_near < 0.1f)
            ortho_near = 0.1f;

        glm::mat4 light_proj = glm::ortho(-radius, radius, -radius, radius, ortho_near, ortho_far);

        // Texel snapping on the final VP
        glm::mat4 shadow_vp = light_proj * light_view;
        float half_sm = (float)KGPU_SHADOW_MAP_SIZE * 0.5f;
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

    if (!m_shadow_se || m_draw_batches.empty())
        return;

    if (m_global_set == VK_NULL_HANDLE || m_objects_set == VK_NULL_HANDLE)
        return;

    auto& current_frame = *m_current_frame;

    auto* se = m_shadow_se;
    if (se->m_failed_load)
        return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, se->m_pipeline);

    // Bind descriptor sets using dummy offsets (SSBOs don't need real offsets)
    const uint32_t dummy_offset[KGPU_objects_max_binding + 1] = {};

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, se->m_pipeline_layout,
                            KGPU_global_descriptor_sets, 1, &m_global_set,
                            current_frame.buffers.dynamic_data.get_dyn_offsets_count(),
                            current_frame.buffers.dynamic_data.get_dyn_offsets_ptr());

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, se->m_pipeline_layout,
                            KGPU_objects_descriptor_sets, 1, &m_objects_set,
                            KGPU_objects_max_binding + 1, dummy_offset);

    // Push constants with cascade index for VP matrix selection
    gpu::push_constants pc = {};
    pc.instance_base = 0;
    pc.directional_light_id = cascade_idx;

    // Draw all opaque batches into shadow map
    for (const auto& batch : m_draw_batches)
    {
        if (!batch.mesh)
            continue;

        // Safe mesh binding — copy handle to local to avoid null-handle Vulkan errors
        VkBuffer vb = batch.mesh->m_vertex_buffer.buffer();
        if (!vb)
            continue;

        VkDeviceSize vb_offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &vb_offset);

        bool indexed = batch.mesh->has_indices();
        if (indexed)
        {
            VkBuffer ib = batch.mesh->m_index_buffer.buffer();
            if (!ib)
                continue;
            vkCmdBindIndexBuffer(cmd, ib, 0, VK_INDEX_TYPE_UINT32);
        }

        pc.instance_base = batch.first_instance_offset;
        vkCmdPushConstants(cmd, se->m_pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(gpu::push_constants), &pc);

        if (indexed)
            vkCmdDrawIndexed(cmd, batch.mesh->indices_size(), batch.instance_count, 0, 0, 0);
        else
            vkCmdDraw(cmd, batch.mesh->vertices_size(), batch.instance_count, 0, 0);
    }
}

// ============================================================================
// Local Light Shadow Drawing
// ============================================================================

void
vulkan_render::draw_shadow_local_pass(VkCommandBuffer cmd, uint32_t shadow_idx, bool back_face)
{
    ZoneScopedN("Render::DrawShadowLocalPass");

    if (m_draw_batches.empty())
        return;

    auto& current_frame = *m_current_frame;

    bool is_point =
        m_shadow_config.local_shadows[shadow_idx].shadow_info.z == KGPU_light_type_point;
    auto* se = is_point ? m_shadow_dpsm_se : m_shadow_se;

    if (!se || se->m_failed_load)
        return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, se->m_pipeline);

    const uint32_t dummy_offset[KGPU_objects_max_binding + 1] = {};

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, se->m_pipeline_layout,
                            KGPU_global_descriptor_sets, 1, &m_global_set,
                            current_frame.buffers.dynamic_data.get_dyn_offsets_count(),
                            current_frame.buffers.dynamic_data.get_dyn_offsets_ptr());

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, se->m_pipeline_layout,
                            KGPU_objects_descriptor_sets, 1, &m_objects_set,
                            KGPU_objects_max_binding + 1, dummy_offset);

    gpu::push_constants pc = {};
    pc.directional_light_id = shadow_idx;             // Encode shadow index
    pc.use_clustered_lighting = back_face ? 1u : 0u;  // Encode hemisphere

    for (const auto& batch : m_draw_batches)
    {
        bind_mesh(cmd, batch.mesh);

        pc.instance_base = batch.first_instance_offset;
        vkCmdPushConstants(cmd, se->m_pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(gpu::push_constants), &pc);

        if (batch.mesh->has_indices())
            vkCmdDrawIndexed(cmd, batch.mesh->indices_size(), batch.instance_count, 0, 0, 0);
        else
            vkCmdDraw(cmd, batch.mesh->vertices_size(), batch.instance_count, 0, 0);
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
            continue;

        float dist = glm::length(light->gpu_data.position - m_camera_data.position);
        if (dist > light->gpu_data.radius * 3.0f)
            continue;  // Too far to matter

        float contribution = light->gpu_data.radius / std::max(dist, 0.1f);
        candidates.push_back({light->slot(), contribution});
    }

    // Sort by contribution (highest first)
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.contribution > b.contribution; });

    // Take top N lights
    uint32_t count =
        std::min((uint32_t)candidates.size(), (uint32_t)KGPU_MAX_SHADOWED_LOCAL_LIGHTS);
    m_shadow_config.shadowed_local_count = count;

    for (uint32_t i = 0; i < count; ++i)
    {
        auto* light = m_cache.universal_lights.at(candidates[i].light_slot);

        auto& shadow = m_shadow_config.local_shadows[i];
        shadow.shadow_info.z = light->gpu_data.type;  // light_type
        float s_near = 0.1f;
        float s_far = light->gpu_data.radius;
        shadow.shadow_params =
            glm::vec4(0.005f,                                           // bias
                      0.02f,                                            // normal_bias
                      1.0f / static_cast<float>(KGPU_SHADOW_MAP_SIZE),  // texel_size
                      s_near                                            // near_plane
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
            shadow.shadow_info.x = m_shadow_local_bindless_indices[i * 2];
            shadow.shadow_info.y = 0xFFFFFFFFu;
        }
        else
        {
            // Point light: dual-paraboloid
            glm::vec3 front_dir(0.0f, 0.0f, 1.0f);
            glm::mat4 view = glm::lookAt(light->gpu_data.position,
                                         light->gpu_data.position + front_dir, glm::vec3(0, 1, 0));
            shadow.view_proj = view;
            shadow.view_proj_back = view;
            shadow.shadow_info.x = m_shadow_local_bindless_indices[i * 2];
            shadow.shadow_info.y = m_shadow_local_bindless_indices[i * 2 + 1];
        }

        // Set shadow_index on the light
        light->gpu_data.shadow_index = i;
    }

    // Clear shadow_index on lights that didn't make the cut
    for (uint32_t i = 0; i < m_cache.universal_lights.get_size(); ++i)
    {
        auto* light = m_cache.universal_lights.at(i);
        if (!light->is_valid())
            continue;

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

    // Compute shadow matrices for this frame
    compute_shadow_matrices();

    // Select which local lights get shadows
    select_shadowed_lights();

    // Upload to GPU
    frame.buffers.shadow_data.begin();
    auto* data = frame.buffers.shadow_data.allocate_data(sizeof(gpu::shadow_config_data));
    memcpy(data, &m_shadow_config, sizeof(gpu::shadow_config_data));
    frame.buffers.shadow_data.end();
}

}  // namespace render
}  // namespace kryga
