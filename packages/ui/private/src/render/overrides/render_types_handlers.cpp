#include "packages/ui/package.ui.h"

#include <render_translator/render_translator.h>
#include <render_translator/render_commands.h>

#include "packages/ui/render/overrides/render_types_handlers.h"
#include "packages/ui/model/ui_panel.h"

#include <glm/glm.hpp>

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

}  // namespace kryga
