#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstdint>

namespace kryga::render
{

// Per-glyph atlas metrics, in pixels at the bake size. uv0/uv1 are the glyph's
// rect in the atlas (0..1). Layout mirrors stb_truetype packedchar: `bearing` is
// the offset from the pen's top-left (xoff,yoff), `advance` moves the pen in x.
struct glyph_metrics
{
    glm::vec2 uv0{0.f};
    glm::vec2 uv1{0.f};
    glm::vec2 size{0.f};     // glyph quad size in px (at bake height)
    glm::vec2 bearing{0.f};  // pen-top-left -> quad-top-left offset, px
    float advance = 0.f;     // pen advance, px
};

// Printable-ASCII font atlas baked from a TTF into one bindless texture.
// Render-side, baked once at init; the draw path lays out glyph quads from the
// metrics. Digits are baked at a uniform (tabular) advance so a changing counter
// does not jitter. Populated by vulkan_render_loader::setup_ui_font().
class font_atlas
{
public:
    static constexpr int k_first_char = 32;  // space
    static constexpr int k_char_count = 95;  // 32..126 inclusive (printable ASCII)
    static constexpr uint32_t k_invalid = 0xFFFFFFFFu;

    const glyph_metrics&
    glyph(char c) const
    {
        int i = static_cast<int>(static_cast<unsigned char>(c)) - k_first_char;
        if (i < 0 || i >= k_char_count)
        {
            i = 0;  // fall back to space for anything out of range
        }
        return m_glyphs[i];
    }

    float
    bake_height() const
    {
        return m_bake_height;
    }
    float
    ascent() const
    {
        return m_ascent;
    }
    float
    line_height() const
    {
        return m_line_height;
    }
    uint32_t
    bindless_index() const
    {
        return m_bindless_index;
    }
    bool
    ready() const
    {
        return m_bindless_index != k_invalid;
    }

    // Filled by vulkan_render_loader::setup_ui_font(); public so the baker writes them
    // directly (this is a render-internal data holder, not an API surface).
    std::array<glyph_metrics, k_char_count> m_glyphs{};
    float m_bake_height = 0.f;
    float m_ascent = 0.f;  // top-of-line -> baseline, px at bake height
    float m_line_height = 0.f;
    uint32_t m_bindless_index = k_invalid;
};

}  // namespace kryga::render
