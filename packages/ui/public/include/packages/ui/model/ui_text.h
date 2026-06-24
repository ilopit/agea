#pragma once

#include "packages/ui/model/ui_text.ar.h"

#include "packages/root/model/ui_widget.h"
#include "packages/root/model/core_types/vec4.h"

#include <render_types/render_handle.h>

#include <utils/id.h>

#include <string>

namespace kryga
{
namespace ui
{

// Anchor of the text block against the viewport. Values MUST match the render
// side (kryga_render::draw_ui_text): right/bottom anchors align the block to that
// edge so e.g. a right-anchored score keeps its right edge fixed as it changes.
enum class ui_text_anchor : int32_t
{
    top_left = 0,
    top_right = 1,
    bottom_left = 2,
    bottom_right = 3,
};

// A single line of UI text. Glyph layout + font-atlas sampling happen render-side
// (the model never reads the viewport or metrics); this widget just carries the
// string, color, pixel font size, and anchor. m_text is a plain runtime member,
// NOT a reflected/serialized property: it is driven by game code each frame (e.g.
// a score), not authored in the editor. Setters mark the widget render-dirty so
// the next frame rebuilds the ui_text_upsert command.
// clang-format off
KRG_ar_class(render_cmd_builder   = ui_text__cmd_builder,
             render_cmd_destroyer = ui_text__cmd_destroyer);
class ui_text : public ::kryga::root::ui_widget
// clang-format on
{
    KRG_gen_meta__ui_text();

public:
    KRG_gen_class_meta(ui_text, ::kryga::root::ui_widget);

    KRG_gen_construct_params{};

    KRG_gen_meta_api;

    bool
    construct(construct_params& c);

    const std::string&
    get_text() const
    {
        return m_text;
    }
    void
    set_text(const std::string& v)
    {
        if (m_text != v)
        {
            m_text = v;
            mark_render_dirty();
        }
    }

    const ::kryga::root::vec4&
    get_color() const
    {
        return m_color;
    }
    void
    set_color(const ::kryga::root::vec4& v)
    {
        m_color = v;
        mark_render_dirty();
    }

    float
    get_font_size() const
    {
        return m_font_size;
    }
    void
    set_font_size(float v)
    {
        m_font_size = v;
        mark_render_dirty();
    }

    int32_t
    get_anchor() const
    {
        return m_anchor;
    }
    void
    set_anchor(ui_text_anchor v)
    {
        m_anchor = static_cast<int32_t>(v);
        mark_render_dirty();
    }

    // Which baked font to render with. Defaults to AID("ui_font_default") — the
    // built-in font the engine bakes (vulkan_engine::init_default_resources) — so
    // callers can ignore it unless they want a specific face. A font id with no
    // registered atlas renders nothing (the draw skips it).
    const utils::id&
    get_font() const
    {
        return m_font;
    }
    void
    set_font(const utils::id& v)
    {
        m_font = v;
        mark_render_dirty();
    }

    // Render-side text-slot handle (loader's ui_text laned_storage). Reserved by
    // the render builder on first build, freed by the destroyer. Null until built.
    // Not serialized — purely a runtime render binding.
    ::kryga::render::types::ui_text_handle&
    text_handle()
    {
        return m_text_handle;
    }

private:
    std::string m_text;
    ::kryga::root::vec4 m_color{1.f, 1.f, 1.f, 1.f};
    float m_font_size = 24.f;
    int32_t m_anchor = static_cast<int32_t>(ui_text_anchor::top_left);
    utils::id m_font = AID("ui_font_default");  // contract: engine bakes this id
    ::kryga::render::types::ui_text_handle m_text_handle;
};

}  // namespace ui
}  // namespace kryga
