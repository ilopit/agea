#pragma once

#include "packages/root/model/texture.ar.h"

#include "packages/root/model/assets/asset.h"

#include <utils/buffer.h>
#include <utils/slot_handle.h>

namespace kryga
{
namespace render
{
class texture_data;
}

namespace root
{

KRG_ar_class("architype=texture",
             render_cmd_builder = texture__cmd_builder,
             render_cmd_destroyer = texture__cmd_destroyer);
class texture : public asset
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

    utils::slot_handle<render::texture_data>
    get_render_handle() const
    {
        return m_render_handle;
    }

    void
    set_render_handle(utils::slot_handle<render::texture_data> h)
    {
        m_render_handle = h;
    }

protected:
    KRG_ar_property("category=meta", "access=all", "serializable=true");
    utils::buffer m_base_color;

    KRG_ar_property("category=meta", "access=all", "serializable=true");
    uint32_t m_width;

    KRG_ar_property("category=meta", "access=all", "serializable=true");
    uint32_t m_height;

    utils::slot_handle<render::texture_data> m_render_handle;
};

}  // namespace root
}  // namespace kryga
