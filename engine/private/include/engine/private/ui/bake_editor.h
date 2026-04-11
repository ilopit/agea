#pragma once

#include "engine/ui.h"

#include <vulkan_render/bake/bake_types.h>
#include <vfs/rid.h>

namespace kryga
{
namespace ui
{

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

private:
    render::bake::bake_config m_config;
    vfs::rid m_cache_rid;
};

}  // namespace ui
}  // namespace kryga
