#include "packages/ui/package.ui.h"

#include <global_state/global_state.h>
#include <render_bridge/render_bridge.h>
#include <render_bridge/render_command.h>

#include "packages/ui/render/overrides/render_types_handlers.h"
#include "packages/ui/model/ui_panel.h"

#include <vulkan_render/kryga_render.h>

#include <utils/kryga_log.h>

#include <algorithm>

namespace kryga
{

namespace
{

// Pixel rect (top-left origin) -> NDC rect (min, max) using the current
// viewport size. Y is flipped because Vulkan NDC points down the screen as
// -y and our pixel convention is +y = down.
glm::vec4
pixels_to_ndc(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t vw, uint32_t vh)
{
    float fvw = static_cast<float>(std::max<uint32_t>(vw, 1));
    float fvh = static_cast<float>(std::max<uint32_t>(vh, 1));

    float x0 = (static_cast<float>(x) / fvw) * 2.f - 1.f;
    float x1 = (static_cast<float>(x + w) / fvw) * 2.f - 1.f;
    float y0 = (static_cast<float>(y) / fvh) * 2.f - 1.f;
    float y1 = (static_cast<float>(y + h) / fvh) * 2.f - 1.f;

    return glm::vec4(x0, y0, x1, y1);
}

struct ui_panel_upsert_cmd : render_cmd::render_command_base
{
    utils::id id;
    int32_t x = 0;
    int32_t y = 0;
    int32_t w = 0;
    int32_t h = 0;
    glm::vec3 color{0.f};
    float opacity = 1.f;
    bool visible = true;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        if (!visible)
        {
            ctx.vr.ui_panel_destroy(id);
            return;
        }

        render::vulkan_render::ui_panel_entry entry;
        entry.rect_ndc =
            pixels_to_ndc(x, y, w, h, ctx.vr.get_viewport_width(), ctx.vr.get_viewport_height());
        entry.color_opacity = glm::vec4(color, opacity);

        ctx.vr.ui_panel_create_or_update(id, entry);
    }
};

struct ui_panel_destroy_cmd : render_cmd::render_command_base
{
    utils::id id;

    void
    execute(render_cmd::render_exec_context& ctx) override
    {
        ctx.vr.ui_panel_destroy(id);
    }
};

}  // namespace

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
    cmd->visible = panel.is_visible();

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
