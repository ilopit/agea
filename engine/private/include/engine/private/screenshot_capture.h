#pragma once

#include <vulkan_render/utils/vulkan_image.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace kryga::engine
{

struct screenshot_region
{
    uint32_t x = 0, y = 0, w = 0, h = 0;
};

struct screenshot_result
{
    std::string image_base64;
    screenshot_region region;
};

class screenshot_capture
{
public:
    screenshot_result
    capture(const screenshot_region& region);

    const screenshot_result&
    get_last() const
    {
        return m_last;
    }

    bool
    has_last() const
    {
        return !m_last.image_base64.empty();
    }

    void
    toggle_selection();

    void
    draw_overlay();

    bool
    is_selecting() const
    {
        return m_selecting;
    }

private:
    struct readback_result
    {
        const uint8_t* data = nullptr;
        uint32_t w = 0, h = 0;
        int stride = 0;
    };

    readback_result
    readback_and_crop(const screenshot_region& region);

    std::string
    encode_png(const readback_result& rb);

    void
    ensure_staging(uint32_t w, uint32_t h);

    render::vk_utils::vulkan_image m_staging;
    uint32_t m_staging_w = 0;
    uint32_t m_staging_h = 0;

    std::vector<uint8_t> m_pixels;
    std::vector<uint8_t> m_cropped;

    bool m_selecting = false;
    bool m_dragging = false;
    glm::vec2 m_drag_start{0.0f};
    glm::vec2 m_drag_end{0.0f};

    screenshot_result m_last{};
};

}  // namespace kryga::engine
