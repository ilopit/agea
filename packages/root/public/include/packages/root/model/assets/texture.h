#pragma once

#include "packages/root/model/texture.ar.h"

#include "packages/root/model/assets/asset.h"

#include <utils/buffer.h>
#include <render_types/render_handle.h>

namespace kryga
{
namespace root
{

// clang-format off
KRG_ar_class(
    "architype=texture",
    render_cmd_builder   = texture__cmd_builder,
    render_cmd_destroyer = texture__cmd_destroyer,
    mcp_schema           = "string:asset_id",
    mcp_hint             = "Image data uploaded to GPU — used by materials for surface color and "
                           "normals"
);
class texture : public asset
// clang-format on
{
    KRG_gen_meta__texture();

public:
    KRG_gen_class_meta(texture, asset);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    utils::buffer&
    get_mutable_base_color()
    {
        return m_base_color;
    }

    // Runtime render slot (handle model). Reserved by the texture cmd builder on
    // first render build; materials resolve their texture slots by this handle.
    // NOT serialized — re-reserved each session from the asset.
    ::kryga::render::types::texture_handle
    render_handle() const
    {
        return m_render_handle;
    }
    void
    set_render_handle(::kryga::render::types::texture_handle h)
    {
        m_render_handle = h;
    }

protected:
    // clang-format off
    KRG_ar_property(
        category     = "meta",
        access       = all,
        serializable = true,
        mcp_hint     = "raw pixel data — read-only at runtime"
    );
    utils::buffer m_base_color;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "meta",
        access       = all,
        serializable = true,
        mcp_hint     = "texture width in pixels"
    );
    uint32_t m_width;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "meta",
        access       = all,
        serializable = true,
        mcp_hint     = "texture height in pixels"
    );
    uint32_t m_height;
    // clang-format on

    ::kryga::render::types::texture_handle m_render_handle = {};  // runtime, not serialized
};

}  // namespace root
}  // namespace kryga
