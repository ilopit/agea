#pragma once

#include "engine/ui.h"

#include <vulkan_render/bake/bake_types.h>
#include <vfs/rid.h>

namespace kryga
{
namespace ui
{

struct bake_scene_info
{
    int static_count = 0;
    int directional_count = 0;
    int local_light_count = 0;
    bool level_loaded = false;
};

class bake_editor : public window
{
public:
    static const char*
    window_title()
    {
        return "Lightmap Baker";
    }

    bake_editor()
        : window(window_title())
    {
    }

    void
    init(const vfs::rid& base, const vfs::rid& cache);

    void
    handle() override;

    void
    save_config();

    render::bake::bake_config&
    get_config()
    {
        return m_config;
    }

    bake_scene_info
    collect_scene_info() const;

    bool
    submit_bake();

private:
    render::bake::bake_config m_config;
};

}  // namespace ui
}  // namespace kryga
