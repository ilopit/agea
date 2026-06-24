#include "packages/ui/package.ui.h"

#include <render_translator/render_translator.h>
#include <render_translator/render_commands.h>

#include "packages/ui/render/overrides/render_types_handlers.h"
#include "packages/ui/model/ui_panel.h"
#include "packages/ui/model/ui_text.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cstring>

namespace kryga
{

// Retained-mode UI panels emit POD commands into the central render queue.
// The actual pixel->NDC conversion and viewport lookup happen render-thread-side
// in process(ui_panel_upsert_cmd, ...) (render_commands.cpp), so the builder only
// forwards the raw pixel rect + color from the model.

result_code
ui_panel__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto& panel = ctx.obj->asr<ui::ui_panel>();

    auto* cmd = ctx.rb->alloc_cmd<ui_panel_upsert_cmd>();
    cmd->id = panel.get_id();
    cmd->x = panel.get_x();
    cmd->y = panel.get_y();
    cmd->w = panel.get_width();
    cmd->h = panel.get_height();
    cmd->color = glm::vec3(panel.get_color().x, panel.get_color().y, panel.get_color().z);
    cmd->opacity = panel.get_opacity();
    cmd->visible = panel.get_visible();

    panel.set_render_built(true);
    ctx.rb->enqueue_cmd(cmd);

    return result_code::ok;
}

result_code
ui_panel__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& panel = ctx.obj->asr<ui::ui_panel>();

    if (panel.get_render_built())
    {
        auto* cmd = ctx.rb->alloc_cmd<ui_panel_destroy_cmd>();
        cmd->id = panel.get_id();
        panel.set_render_built(false);
        ctx.rb->enqueue_cmd(cmd);
    }

    return result_code::ok;
}

result_code
ui_text__cmd_builder(reflection::type_context__render_cmd_build& ctx)
{
    auto& text = ctx.obj->asr<ui::ui_text>();

    // Handle model: reserve the render text-slot on first build (the allocator's
    // valid() is false for a null/stale handle). It persists across rebuilds and
    // is freed by the destroyer.
    auto& alloc = ctx.rb->ui_texts_alloc();
    if (!alloc.valid(text.text_handle()))
    {
        text.text_handle() = alloc.reserve();
    }

    auto* cmd = ctx.rb->alloc_cmd<ui_text_upsert_cmd>();
    cmd->handle = text.text_handle();
    cmd->x = text.get_x();
    cmd->y = text.get_y();
    cmd->anchor = static_cast<uint32_t>(text.get_anchor());
    cmd->font_size = text.get_font_size();
    cmd->color = text.get_color().as_glm();
    cmd->font = text.get_font();
    cmd->visible = text.get_visible();

    const std::string& s = text.get_text();
    const size_t n = std::min(s.size(), static_cast<size_t>(ui_text_upsert_cmd::k_max_len - 1));
    std::memcpy(cmd->text, s.data(), n);
    cmd->text[n] = '\0';

    ctx.rb->enqueue_cmd(cmd);

    return result_code::ok;
}

result_code
ui_text__cmd_destroyer(reflection::type_context__render_cmd_build& ctx)
{
    auto& text = ctx.obj->asr<ui::ui_text>();

    auto& alloc = ctx.rb->ui_texts_alloc();
    if (alloc.valid(text.text_handle()))
    {
        auto* cmd = ctx.rb->alloc_cmd<ui_text_destroy_cmd>();
        cmd->handle = text.text_handle();
        ctx.rb->enqueue_cmd(cmd);

        alloc.free(text.text_handle());  // deferred free (model thread)
        text.text_handle() = {};
    }

    return result_code::ok;
}

}  // namespace kryga
