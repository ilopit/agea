#pragma once

#include "packages/root/model/asset.ar.h"

#include "packages/root/model/smart_object.h"

namespace kryga
{
namespace root
{
// clang-format off
KRG_ar_class(
    mcp_hint = "Shared resource loaded from a package — materials / meshes / textures / samplers / "
               "shaders"
);
class asset : public smart_object
// clang-format on
{
    KRG_gen_meta__asset();

public:
    KRG_gen_class_meta(asset, smart_object);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    void
    mark_render_dirty();
};
}  // namespace root
}  // namespace kryga