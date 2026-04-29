#pragma once

#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/types/vulkan_render_pass.h>
#include <vulkan_render/utils/vulkan_image.h>
#include <vulkan_render/utils/vulkan_initializers.h>

#include <global_state/global_state.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace kryga::render::test
{

// Read back entire color attachment from a render pass as RGBA pixels.
// The image must be created with VK_IMAGE_USAGE_TRANSFER_SRC_BIT.
// src_layout: the layout the image is currently in. The function transitions
// it to TRANSFER_SRC_OPTIMAL before the copy.
inline std::vector<uint8_t>
readback_framebuffer(render_pass& pass,
                     uint32_t width,
                     uint32_t height,
                     VkImageLayout src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
{
    auto& device = glob::glob_state().getr_render_device();
    auto frame_idx = device.get_current_frame_index();
    auto color_images = pass.get_color_images();
    auto img_idx = frame_idx % color_images.size();
    auto src_image = color_images[img_idx]->image();

    auto extent = VkExtent3D{width, height, 1};

    // Create linear-tiled destination image for CPU readback
    auto image_ci = vk_utils::make_image_create_info(VK_FORMAT_R8G8B8A8_UNORM, 0, extent);
    image_ci.imageType = VK_IMAGE_TYPE_2D;
    image_ci.arrayLayers = 1;
    image_ci.mipLevels = 1;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci.tiling = VK_IMAGE_TILING_LINEAR;
    image_ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo vma_allocinfo = {};
    vma_allocinfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    vma_allocinfo.requiredFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    auto dst_image = vk_utils::vulkan_image::create(
        device.get_vma_allocator_provider(), image_ci, vma_allocinfo);

    device.immediate_submit(
        [&](VkCommandBuffer cmd)
        {
            // Transition source to TRANSFER_SRC if not already
            if (src_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
            {
                vk_utils::make_insert_image_memory_barrier(
                    cmd,
                    src_image,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    src_layout,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
            }

            // Transition destination to TRANSFER_DST
            vk_utils::make_insert_image_memory_barrier(
                cmd,
                dst_image.image(),
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

            VkImageCopy region{};
            region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.srcSubresource.layerCount = 1;
            region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.dstSubresource.layerCount = 1;
            region.extent = extent;

            vkCmdCopyImage(cmd,
                           src_image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           dst_image.image(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);

            // Transition destination to GENERAL for mapping
            vk_utils::make_insert_image_memory_barrier(
                cmd,
                dst_image.image(),
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_MEMORY_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
        });

    auto* data = dst_image.map();
    std::vector<uint8_t> pixels(width * height * 4);
    std::memcpy(pixels.data(), data, pixels.size());

    // Handle B8G8R8A8 → R8G8B8A8 swizzle (main pass uses BGRA swapchain format)
    if (pass.get_color_format() == VK_FORMAT_B8G8R8A8_UNORM ||
        pass.get_color_format() == VK_FORMAT_B8G8R8A8_SRGB)
    {
        for (uint32_t i = 0; i < width * height; ++i)
        {
            std::swap(pixels[i * 4 + 0], pixels[i * 4 + 2]);
        }
    }
    dst_image.unmap();

    return pixels;
}

}  // namespace kryga::render::test
