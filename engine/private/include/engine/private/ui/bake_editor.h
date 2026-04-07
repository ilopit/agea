#pragma once

#include "engine/ui.h"

#include <vulkan_render/bake/bake_types.h>

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
    handle() override;

private:
    // Editable settings
    int m_resolution = 1024;
    int m_samples = 64;
    int m_bounces = 2;
    int m_denoise = 2;
    float m_ao_radius = 2.0f;
    float m_ao_intensity = 1.0f;
    bool m_bake_direct = true;
    bool m_bake_indirect = true;
    bool m_bake_ao = true;
    bool m_save_png = true;
};

}  // namespace ui
}  // namespace kryga
