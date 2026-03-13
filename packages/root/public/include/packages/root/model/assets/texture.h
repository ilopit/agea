#pragma once

#include "packages/root/model/texture.ar.h"

#include "packages/root/model/assets/asset.h"

#include <utils/buffer.h>

namespace kryga
{
namespace root
{

KRG_ar_class("architype=texture",
              render_cmd_builder   = texture__cmd_builder,
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

protected:
    KRG_ar_property("category=meta", "access=all", "serializable=true");
    utils::buffer m_base_color;

    KRG_ar_property("category=meta", "access=all", "serializable=true");
    uint32_t m_width;

    KRG_ar_property("category=meta", "access=all", "serializable=true");
    uint32_t m_height;
};

}  // namespace root
}  // namespace kryga
