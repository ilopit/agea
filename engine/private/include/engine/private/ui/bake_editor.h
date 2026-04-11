#pragma once

#include "engine/ui.h"

#include <vulkan_render/bake/bake_types.h>
#include <utils/path.h>

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
    init(const utils::path& config_path);

    void
    handle() override;

    void
    save_config();

private:
    render::bake::bake_config m_config;
    utils::path m_config_path;
};

}  // namespace ui
}  // namespace kryga
