#include "vulkan_render/vulkan_render_device.h"
#include "vulkan_render/utils/offscreen_renderer.h"

#include "vulkan_render/types/vulkan_mesh_data.h"
#include "vulkan_render/types/vulkan_render_pass.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"
#include "vulkan_render/utils/vulkan_initializers.h"

#include <gpu_types/gpu_push_constants_main.h>

#include <utils/check.h>

#include <algorithm>
#include <cstring>

namespace kryga::render
{

void
offscreen_renderer::init(render_device& device, render_pass* pass, uint32_t size)
{
    if (m_initialized && m_size == size)
    {
        return;
    }

    KRG_check(pass, "render pass must exist");

    m_vk_pass = pass->vk();
    m_color_format = pass->get_color_format();
    auto depth_format = pass->get_depth_format();
    m_size = size;

    auto vma_provider = device.get_vma_allocator_provider();
    auto vk_device = device.vk_device();

    {
        auto ici = vk_utils::make_image_create_info(
            m_color_format,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VkExtent3D{size, size, 1});
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.arrayLayers = 1;
        ici.mipLevels = 1;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;

        VmaAllocationCreateInfo aci{};
        aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        m_color_image = std::make_shared<vk_utils::vulkan_image>(
            vk_utils::vulkan_image::create(vma_provider, ici, aci, 0, "offscreen_color"));

        auto iv_ci = vk_utils::make_imageview_create_info(
            m_color_format, m_color_image->image(), VK_IMAGE_ASPECT_COLOR_BIT);
        m_color_view = vk_utils::vulkan_image_view::create_shared(iv_ci, "offscreen_color_view");
    }

    {
        auto ici = vk_utils::make_image_create_info(
            depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VkExtent3D{size, size, 1});
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.arrayLayers = 1;
        ici.mipLevels = 1;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;

        VmaAllocationCreateInfo aci{};
        aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        m_depth_image =
            vk_utils::vulkan_image::create(vma_provider, ici, aci, 0, "offscreen_depth");

        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            depth_format == VK_FORMAT_D24_UNORM_S8_UINT)
        {
            aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        auto iv_ci =
            vk_utils::make_imageview_create_info(depth_format, m_depth_image.image(), aspect);
        m_depth_view = vk_utils::vulkan_image_view::create_shared(iv_ci, "offscreen_depth_view");
    }

    {
        auto fb_ci = vk_utils::make_framebuffer_create_info(m_vk_pass, VkExtent2D{size, size});
        VkImageView atts[2] = {m_color_view->vk(), m_depth_view->vk()};
        fb_ci.pAttachments = atts;
        fb_ci.attachmentCount = 2;
        VK_CHECK(vkCreateFramebuffer(vk_device, &fb_ci, nullptr, &m_framebuffer));
    }

    auto make_buf = [&](VkDeviceSize sz, VkBufferUsageFlags usage, const char* name)
    { return device.create_buffer(sz, usage, VMA_MEMORY_USAGE_CPU_TO_GPU, 0, name); };

    m_camera_buf = make_buf(sizeof(gpu::camera_data),
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                            "offscreen_camera");
    m_objects_buf =
        make_buf(sizeof(gpu::object_data), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "offscreen_objects");
    m_slots_buf = make_buf(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "offscreen_slots");
    m_dir_lights_buf = make_buf(sizeof(gpu::directional_light_data),
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                "offscreen_lights");
    m_material_buf = make_buf(256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "offscreen_material");
    m_dummy_buf = make_buf(256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "offscreen_dummy");

    m_initialized = true;
}

void
offscreen_renderer::destroy(VkDevice device)
{
    if (!m_initialized)
    {
        return;
    }

    if (m_framebuffer != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(device, m_framebuffer, nullptr);
        m_framebuffer = VK_NULL_HANDLE;
    }

    m_color_view.reset();
    m_color_image.reset();
    m_depth_view.reset();
    m_depth_image.clear();

    m_camera_buf.clear();
    m_objects_buf.clear();
    m_slots_buf.clear();
    m_dir_lights_buf.clear();
    m_material_buf.clear();
    m_dummy_buf.clear();

    m_initialized = false;
}

offscreen_render_result
offscreen_renderer::render(render_device& device, const offscreen_draw_request& request)
{
    KRG_check(m_initialized, "offscreen_renderer not initialized");
    KRG_check(request.mesh, "mesh required");
    KRG_check(request.shader_effect, "shader_effect required");
    KRG_check(request.bindless_set != VK_NULL_HANDLE, "bindless_set required");

    auto* se = request.shader_effect;
    auto* mesh = request.mesh;
    uint32_t size = m_size;

    m_camera_buf.begin();
    m_objects_buf.begin();
    m_slots_buf.begin();
    m_dir_lights_buf.begin();
    m_material_buf.begin();
    m_dummy_buf.begin();

    std::memcpy(m_camera_buf.get_data(), &request.camera, sizeof(gpu::camera_data));
    std::memcpy(m_objects_buf.get_data(), &request.object, sizeof(gpu::object_data));

    uint32_t slot = 0;
    std::memcpy(m_slots_buf.get_data(), &slot, sizeof(slot));

    std::memcpy(m_dir_lights_buf.get_data(),
                &request.directional_light,
                sizeof(gpu::directional_light_data));

    if (request.material_gpu_data && request.material_gpu_data_size > 0)
    {
        size_t copy_sz = (std::min)(request.material_gpu_data_size, size_t(256));
        std::memcpy(m_material_buf.get_data(), request.material_gpu_data, copy_sz);
    }

    std::memset(m_dummy_buf.get_data(), 0, 256);

    m_camera_buf.end();
    m_objects_buf.end();
    m_slots_buf.end();
    m_dir_lights_buf.end();
    m_material_buf.end();
    m_dummy_buf.end();

    gpu::push_constants_main pc{};
    pc.material_id = 0;
    pc.directional_light_id = 0;
    pc.use_clustered_lighting = 0;
    pc.instance_base = 0;
    pc.enable_directional_light = 1;
    pc.enable_local_lights = 0;
    pc.enable_baked_light = 0;

    for (uint32_t i = 0; i < KGPU_MAX_TEXTURE_SLOTS; ++i)
    {
        pc.texture_indices[i] = request.texture_indices[i];
        pc.sampler_indices[i] = request.sampler_indices[i];
    }

    auto dummy_bda = gpu::make_bda_addr(m_dummy_buf.device_address());
    pc.bdag_camera = gpu::make_bda_addr(m_camera_buf.device_address());
    pc.bdag_objects = gpu::make_bda_addr(m_objects_buf.device_address());
    pc.bdag_directional_lights = gpu::make_bda_addr(m_dir_lights_buf.device_address());
    pc.bdag_universal_lights = dummy_bda;
    pc.bdag_cluster_counts = dummy_bda;
    pc.bdag_cluster_indices = dummy_bda;
    pc.bdag_cluster_config = dummy_bda;
    pc.bdag_instance_slots = gpu::make_bda_addr(m_slots_buf.device_address());
    pc.bdag_bone_matrices = dummy_bda;
    pc.bdag_shadow_data = dummy_bda;
    pc.bdag_probe_data = dummy_bda;
    pc.bdag_probe_grid = dummy_bda;
    pc.bdaf_material = gpu::make_bda_addr(m_material_buf.device_address());

    auto staging_ici =
        vk_utils::make_image_create_info(VK_FORMAT_R8G8B8A8_UNORM, 0, VkExtent3D{size, size, 1});
    staging_ici.imageType = VK_IMAGE_TYPE_2D;
    staging_ici.arrayLayers = 1;
    staging_ici.mipLevels = 1;
    staging_ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    staging_ici.samples = VK_SAMPLE_COUNT_1_BIT;
    staging_ici.tiling = VK_IMAGE_TILING_LINEAR;
    staging_ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo staging_aci{};
    staging_aci.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    staging_aci.requiredFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    auto staging = vk_utils::vulkan_image::create(
        device.get_vma_allocator_provider(), staging_ici, staging_aci, 0, "offscreen_staging");

    VkImage color_img = m_color_image->image();
    VkDescriptorSet bindless_set = request.bindless_set;

    device.immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            vk_utils::make_insert_image_memory_barrier(
                cmd,
                color_img,
                0,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

            VkImageAspectFlags depth_aspect =
                VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            vk_utils::make_insert_image_memory_barrier(
                cmd,
                m_depth_image.image(),
                0,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                VkImageSubresourceRange{depth_aspect, 0, 1, 0, 1});

            VkRenderPassBeginInfo rp_bi{};
            rp_bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_bi.renderPass = m_vk_pass;
            rp_bi.framebuffer = m_framebuffer;
            rp_bi.renderArea = VkRect2D{{0, 0}, {size, size}};
            VkClearValue clears[2]{};
            clears[0].color = {{request.clear_color[0],
                                request.clear_color[1],
                                request.clear_color[2],
                                request.clear_color[3]}};
            clears[1].depthStencil = {1.0f, 0};
            rp_bi.clearValueCount = 2;
            rp_bi.pClearValues = clears;
            vkCmdBeginRenderPass(cmd, &rp_bi, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport vp{0.0f, 0.0f, float(size), float(size), 0.0f, 1.0f};
            vkCmdSetViewport(cmd, 0, 1, &vp);
            VkRect2D scissor{{0, 0}, {size, size}};
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, se->m_pipeline);

            vkCmdBindDescriptorSets(cmd,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    se->m_pipeline_layout,
                                    KGPU_textures_descriptor_sets,
                                    1,
                                    &bindless_set,
                                    0,
                                    nullptr);

            se->push_constants(cmd, &pc);

            VkDeviceSize vb_offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->m_vertex_buffer.buffer(), &vb_offset);
            if (mesh->has_indices())
            {
                vkCmdBindIndexBuffer(cmd, mesh->m_index_buffer.buffer(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cmd, mesh->indices_size(), 1, 0, 0, 0);
            }
            else
            {
                vkCmdDraw(cmd, mesh->vertices_size(), 1, 0, 0);
            }

            vkCmdEndRenderPass(cmd);

            vk_utils::make_insert_image_memory_barrier(
                cmd,
                color_img,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

            vk_utils::make_insert_image_memory_barrier(
                cmd,
                staging.image(),
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

            VkImageCopy region{};
            region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.extent = {size, size, 1};
            vkCmdCopyImage(cmd,
                           color_img,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           staging.image(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);
        });

    auto* mapped = staging.map();
    KRG_check(mapped, "failed to map staging image");

    VkImageSubresource subres{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
    VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(device.vk_device(), staging.image(), &subres, &layout);

    offscreen_render_result result;
    result.width = size;
    result.height = size;
    result.pixels.resize(size * size * 4);

    for (uint32_t row = 0; row < size; ++row)
    {
        auto* src = static_cast<uint8_t*>(mapped) + layout.offset + row * layout.rowPitch;
        auto* dst = result.pixels.data() + row * size * 4;
        std::memcpy(dst, src, size * 4);
    }

    staging.unmap();

    if (m_color_format == VK_FORMAT_B8G8R8A8_UNORM || m_color_format == VK_FORMAT_B8G8R8A8_SRGB)
    {
        for (uint32_t i = 0; i < size * size; ++i)
        {
            std::swap(result.pixels[i * 4 + 0], result.pixels[i * 4 + 2]);
        }
    }

    return result;
}

}  // namespace kryga::render
