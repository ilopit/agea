#include "engine/private/screenshot_capture.h"

#include <global_state/global_state.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/render_system.h>
#include <vulkan_render/utils/vulkan_initializers.h>

#include <utils/base64.h>
#include <vfs/vfs.h>

#include <stb_unofficial/stb.h>
#include <imgui.h>

#include <algorithm>
#include <cstring>

namespace kryga::engine
{

namespace
{

void
png_write_cb(void* ctx, void* data, int size)
{
    auto* buf = static_cast<std::vector<uint8_t>*>(ctx);
    auto* bytes = static_cast<uint8_t*>(data);
    buf->insert(buf->end(), bytes, bytes + size);
}

}  // namespace

void
screenshot_capture::ensure_staging(uint32_t w, uint32_t h)
{
    if (m_staging_w == w && m_staging_h == h && m_staging.image() != VK_NULL_HANDLE)
    {
        return;
    }

    auto& device = glob::glob_state().getr_render().device;

    auto extent = VkExtent3D{w, h, 1};
    auto ci = render::vk_utils::make_image_create_info(VK_FORMAT_R8G8B8A8_UNORM, 0, extent);
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.arrayLayers = 1;
    ci.mipLevels = 1;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_LINEAR;
    ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo vma_ai = {};
    vma_ai.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    vma_ai.requiredFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    m_staging =
        render::vk_utils::vulkan_image::create(device.get_vma_allocator_provider(), ci, vma_ai);
    m_staging_w = w;
    m_staging_h = h;
    m_pixels.resize(w * h * 4);
}

screenshot_capture::readback_result
screenshot_capture::readback_and_crop(const screenshot_region& region)
{
    auto& renderer = glob::glob_state().getr_render().renderer;
    auto& device = glob::glob_state().getr_render().device;

    auto pass_id = renderer.get_host_pass_id();
    auto* pass = renderer.get_render_pass(pass_id);
    if (!pass)
    {
        return {};
    }

    uint32_t w = renderer.get_width();
    uint32_t h = renderer.get_height();

    ensure_staging(w, h);

    auto color_images = pass->get_color_images();
    // Sample the image that was actually presented (acquired index), not
    // frame_slot % count — those diverge under MAILBOX, which would read a
    // stale/unrendered image.
    auto img_idx = device.last_presented_image_index() % color_images.size();
    auto src_image = color_images[img_idx]->image();
    VkImage dst_vk = m_staging.image();

    auto extent = VkExtent3D{w, h, 1};

    device.immediate_submit(
        [&, dst_vk](VkCommandBuffer cmd)
        {
            render::vk_utils::make_insert_image_memory_barrier(
                cmd,
                src_image,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

            render::vk_utils::make_insert_image_memory_barrier(
                cmd,
                dst_vk,
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

            VkImageCopy copy{};
            copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.extent = extent;

            vkCmdCopyImage(cmd,
                           src_image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           dst_vk,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &copy);

            render::vk_utils::make_insert_image_memory_barrier(
                cmd,
                dst_vk,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_MEMORY_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

            render::vk_utils::make_insert_image_memory_barrier(
                cmd,
                src_image,
                VK_ACCESS_TRANSFER_READ_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
        });

    uint8_t* mapped = m_staging.map();
    std::memcpy(m_pixels.data(), mapped, m_pixels.size());
    m_staging.unmap();

    auto fmt = pass->get_color_format();
    if (fmt == VK_FORMAT_B8G8R8A8_UNORM || fmt == VK_FORMAT_B8G8R8A8_SRGB)
    {
        for (uint32_t i = 0; i < w * h; ++i)
        {
            std::swap(m_pixels[i * 4 + 0], m_pixels[i * 4 + 2]);
        }
    }

    uint32_t out_x = region.x, out_y = region.y;
    uint32_t out_w = (region.w > 0) ? region.w : w;
    uint32_t out_h = (region.h > 0) ? region.h : h;
    out_w = std::min(out_w, w - std::min(out_x, w));
    out_h = std::min(out_h, h - std::min(out_y, h));

    const uint8_t* src_ptr = m_pixels.data();
    int stride = int(w * 4);

    if (out_x != 0 || out_y != 0 || out_w != w || out_h != h)
    {
        m_cropped.resize(out_w * out_h * 4);
        for (uint32_t row = 0; row < out_h; ++row)
        {
            std::memcpy(m_cropped.data() + row * out_w * 4,
                        m_pixels.data() + (out_y + row) * w * 4 + out_x * 4,
                        out_w * 4);
        }
        src_ptr = m_cropped.data();
        stride = int(out_w * 4);
    }

    return {src_ptr, out_w, out_h, stride};
}

std::string
screenshot_capture::encode_png(const readback_result& rb)
{
    std::vector<uint8_t> png_buf;
    stbi_write_png_to_func(png_write_cb, &png_buf, int(rb.w), int(rb.h), 4, rb.data, rb.stride);
    if (png_buf.empty())
    {
        return {};
    }
    return "data:image/png;base64," + base64_encode(png_buf.data(), png_buf.size());
}

screenshot_result
screenshot_capture::capture(const screenshot_region& region)
{
    auto rb = readback_and_crop(region);
    if (!rb.data)
    {
        return {};
    }

    screenshot_result sr;
    sr.image_base64 = encode_png(rb);
    sr.region = {region.x, region.y, rb.w, rb.h};
    return sr;
}

void
screenshot_capture::release()
{
    // Move-assigning a default image clears the old one (vmaDestroyImage via
    // its stored allocator). Safe to call when nothing was ever captured —
    // m_staging is then already empty.
    m_staging = render::vk_utils::vulkan_image{};
    m_staging_w = 0;
    m_staging_h = 0;
    m_pixels.clear();
    m_cropped.clear();
}

void
screenshot_capture::toggle_selection()
{
    m_selecting = !m_selecting;
    m_dragging = false;
}

void
screenshot_capture::draw_overlay()
{
    if (!m_selecting)
    {
        return;
    }

    ImGui::GetIO().WantCaptureMouse = true;

    auto mouse = ImGui::GetMousePos();

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_dragging = true;
        m_drag_start = {mouse.x, mouse.y};
        m_drag_end = m_drag_start;
    }

    if (m_dragging)
    {
        m_drag_end = {mouse.x, mouse.y};
    }

    if (m_dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        m_dragging = false;
        m_selecting = false;

        float x0 = glm::min(m_drag_start.x, m_drag_end.x);
        float y0 = glm::min(m_drag_start.y, m_drag_end.y);
        float x1 = glm::max(m_drag_start.x, m_drag_end.x);
        float y1 = glm::max(m_drag_start.y, m_drag_end.y);

        screenshot_region region{uint32_t(glm::max(0.0f, x0)),
                                 uint32_t(glm::max(0.0f, y0)),
                                 uint32_t(glm::max(1.0f, x1 - x0)),
                                 uint32_t(glm::max(1.0f, y1 - y0))};

        m_last = capture(region);
    }

    auto* dl = ImGui::GetForegroundDrawList();

    if (m_dragging)
    {
        ImVec2 p0{m_drag_start.x, m_drag_start.y};
        ImVec2 p1{m_drag_end.x, m_drag_end.y};
        dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 30));
        dl->AddRect(p0, p1, IM_COL32(255, 80, 80, 200), 0.0f, 0, 4.0f);
    }

    dl->AddText(ImVec2(10, 10),
                IM_COL32(255, 80, 80, 255),
                "SCREENSHOT SELECT — drag to capture, H to cancel");
}

}  // namespace kryga::engine
