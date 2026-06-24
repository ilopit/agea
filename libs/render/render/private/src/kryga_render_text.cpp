#include "vulkan_render/kryga_render.h"
#include "vulkan_render/font_atlas.h"
#include "vulkan_render/vulkan_render_loader.h"
#include "vulkan_render/render_system.h"
#include "vulkan_render/types/vulkan_shader_effect_data.h"

#include <global_state/global_state.h>
#include <vfs/vfs.h>

#include <gpu_types/gpu_generic_constants.h>

#include <utils/id.h>

#include <utils/buffer.h>
#include <utils/kryga_log.h>

// stb_truetype implementation lives in THIS TU only. stb.cpp (stb_unofficial)
// compiles stb_image / stb_image_write but NOT truetype, so there is no clash.
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace kryga::render
{

// Bake a TTF into a printable-ASCII atlas under `id`: rasterize the glyphs into an
// R8 coverage atlas, capture per-glyph metrics, then upload an RGBA8 (white,
// coverage in alpha) texture and record its bindless index for the text shader.
// The TTF is read through the VFS. Independent of ImGui, so it works in game
// builds. Owned by the loader like any other render resource; the bindless upload
// goes through the renderer.
font_atlas*
vulkan_render_loader::load_font(const kryga::utils::id& id,
                                std::string_view ttf_path,
                                float bake_height)
{
    std::vector<uint8_t> ttf;
    if (!glob::glob_state().getr_vfs().read_bytes(vfs::rid(std::string(ttf_path)), ttf))
    {
        ALOG_ERROR("UI font '{}': failed to read {}", id.str(), std::string(ttf_path));
        return nullptr;
    }

    constexpr int k_atlas_w = 512;
    constexpr int k_atlas_h = 512;
    const float k_bake_height = bake_height;  // bake large; the draw scales down

    std::vector<uint8_t> coverage(static_cast<size_t>(k_atlas_w) * k_atlas_h, 0);
    std::array<stbtt_packedchar, font_atlas::k_char_count> packed{};

    stbtt_pack_context pc;
    if (!stbtt_PackBegin(&pc, coverage.data(), k_atlas_w, k_atlas_h, 0, 1, nullptr))
    {
        ALOG_ERROR("UI font '{}': stbtt_PackBegin failed", id.str());
        return nullptr;
    }
    // No oversampling: keep glyph atlas rects in 1x px so size (x1-x0) stays
    // consistent with xadvance/xoff (both 1x). The 48px bake downscaled to the
    // render size via the linear sampler is plenty smooth. (Oversampling would
    // need stbtt_GetPackedQuad-style /oversample handling on the rects.)
    stbtt_PackSetOversampling(&pc, 1, 1);
    stbtt_PackFontRange(&pc,
                        ttf.data(),
                        0,
                        k_bake_height,
                        font_atlas::k_first_char,
                        font_atlas::k_char_count,
                        packed.data());
    stbtt_PackEnd(&pc);

    // Vertical metrics for line height.
    stbtt_fontinfo info;
    stbtt_InitFont(&info, ttf.data(), stbtt_GetFontOffsetForIndex(ttf.data(), 0));
    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    const float vscale = stbtt_ScaleForPixelHeight(&info, k_bake_height);

    font_atlas atlas;
    atlas.m_bake_height = k_bake_height;
    atlas.m_ascent = ascent * vscale;
    atlas.m_line_height = (ascent - descent + line_gap) * vscale;

    // Tabular digits: bake every digit to the widest digit's advance and center
    // the glyph in that cell, so a changing counter does not jitter.
    float max_digit_adv = 0.f;
    for (char d = '0'; d <= '9'; ++d)
    {
        max_digit_adv = std::max(max_digit_adv, packed[d - font_atlas::k_first_char].xadvance);
    }

    for (int i = 0; i < font_atlas::k_char_count; ++i)
    {
        const stbtt_packedchar& s = packed[i];
        glyph_metrics& g = atlas.m_glyphs[i];
        g.uv0 = {s.x0 / static_cast<float>(k_atlas_w), s.y0 / static_cast<float>(k_atlas_h)};
        g.uv1 = {s.x1 / static_cast<float>(k_atlas_w), s.y1 / static_cast<float>(k_atlas_h)};
        g.size = {static_cast<float>(s.x1 - s.x0), static_cast<float>(s.y1 - s.y0)};
        g.bearing = {s.xoff, s.yoff};
        g.advance = s.xadvance;

        const char c = static_cast<char>(font_atlas::k_first_char + i);
        if (c >= '0' && c <= '9')
        {
            g.bearing.x += (max_digit_adv - g.advance) * 0.5f;
            g.advance = max_digit_adv;
        }
    }

    // Expand R8 coverage -> RGBA8 (white, coverage in alpha) for create_texture.
    kryga::utils::buffer rgba;
    rgba.resize(static_cast<size_t>(k_atlas_w) * k_atlas_h * 4);
    auto* dst = reinterpret_cast<uint8_t*>(rgba.data());
    for (int i = 0; i < k_atlas_w * k_atlas_h; ++i)
    {
        dst[i * 4 + 0] = 255;
        dst[i * 4 + 1] = 255;
        dst[i * 4 + 2] = 255;
        dst[i * 4 + 3] = coverage[i];
    }

    // Per-font texture debug name (ids are debug-only now) so atlases are
    // distinguishable in logs/captures.
    texture_data* td = glob::glob_state().getr_render().renderer.create_texture(
        AID(("ui_font_atlas:" + id.str()).c_str()), rgba, k_atlas_w, k_atlas_h);
    if (!td)
    {
        ALOG_ERROR("UI font '{}': create_texture failed", id.str());
        return nullptr;
    }
    atlas.m_bindless_index = td->get_bindless_index();

    font_atlas& stored = (m_fonts[id] = atlas);
    ALOG_INFO("UI font '{}' baked: {}x{} bindless={}",
              id.str(),
              k_atlas_w,
              k_atlas_h,
              stored.m_bindless_index);
    return &stored;
}

// Lay out and draw every text entry: one glyph quad per character, sized from the
// baked metrics and placed against the live viewport per the entry's anchor. Pen
// math is in pixels (top-left origin), converted to NDC per glyph. Right/bottom
// anchors align the text block to that edge so a counter's right edge stays put.
void
vulkan_render::draw_ui_text(VkCommandBuffer cmd)
{
    auto& storage = m_loader->ui_texts_storage();
    if (!m_ui_text_se || storage.active() == 0)
    {
        return;
    }

    const float vw = static_cast<float>(get_width() == 0 ? 1u : get_width());
    const float vh = static_cast<float>(get_height() == 0 ? 1u : get_height());

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ui_text_se->m_pipeline);
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_ui_text_se->m_pipeline_layout,
                            KGPU_textures_descriptor_sets,
                            1,
                            &m_bindless_set,
                            0,
                            nullptr);

    struct ui_text_pc
    {
        glm::vec4 rect_ndc;
        glm::vec4 uv_rect;
        glm::vec4 color;
        uint32_t tex_index;
        uint32_t sampler_index;
    };

    const VkPipelineLayout pipeline_layout = m_ui_text_se->m_pipeline_layout;

    // Single-lane storage (render_translator's content allocator owns lane 0).
    // Iterate the live slots; the handle generations don't matter for the draw —
    // an occupied slot holds a populated entry.
    auto& lane = storage.lane(0);
    const uint32_t slot_count = static_cast<uint32_t>(lane.size());

    for (uint32_t slot = 0; slot < slot_count; ++slot)
    {
        if (!lane.occupied(slot))
        {
            continue;
        }
        const ui_text_entry& e = *lane.at(slot);

        if (e.text.empty())
        {
            continue;
        }

        // Resolve this entry's font (empty/unknown id -> loader default). Skip if
        // even the default has not been baked.
        font_atlas* fa = m_loader->get_font(e.font);
        if (!fa || !fa->ready())
        {
            continue;
        }
        const font_atlas& font = *fa;
        const uint32_t tex_index = font.bindless_index();

        const float scale = e.font_size / font.bake_height();

        float text_w = 0.f;
        for (char ch : e.text)
        {
            text_w += font.glyph(ch).advance * scale;
        }

        const bool right = (e.anchor == 1 || e.anchor == 3);
        const bool bottom = (e.anchor == 2 || e.anchor == 3);

        const float anchor_x = right ? (vw - static_cast<float>(e.x)) : static_cast<float>(e.x);
        const float anchor_y = bottom ? (vh - static_cast<float>(e.y)) : static_cast<float>(e.y);

        float pen_x = right ? (anchor_x - text_w) : anchor_x;
        const float top_y = bottom ? (anchor_y - font.line_height() * scale) : anchor_y;
        const float baseline_y = top_y + font.ascent() * scale;

        for (char ch : e.text)
        {
            const glyph_metrics& g = font.glyph(ch);

            const float qx0 = pen_x + g.bearing.x * scale;
            const float qy0 = baseline_y + g.bearing.y * scale;
            const float qx1 = qx0 + g.size.x * scale;
            const float qy1 = qy0 + g.size.y * scale;

            pen_x += g.advance * scale;

            // Whitespace/empty glyphs (e.g. space) have a zero-area quad — advance
            // the pen but emit no draw.
            if (g.size.x <= 0.f || g.size.y <= 0.f)
            {
                continue;
            }

            ui_text_pc pc;
            pc.rect_ndc = glm::vec4(qx0 / vw * 2.f - 1.f,
                                    qy0 / vh * 2.f - 1.f,
                                    qx1 / vw * 2.f - 1.f,
                                    qy1 / vh * 2.f - 1.f);
            pc.uv_rect = glm::vec4(g.uv0.x, g.uv0.y, g.uv1.x, g.uv1.y);
            pc.color = e.color;
            pc.tex_index = tex_index;
            pc.sampler_index = KGPU_SAMPLER_LINEAR_CLAMP;

            vkCmdPushConstants(cmd,
                               pipeline_layout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(ui_text_pc),
                               &pc);
            vkCmdDraw(cmd, 6, 1, 0, 0);
        }
    }
}

}  // namespace kryga::render
