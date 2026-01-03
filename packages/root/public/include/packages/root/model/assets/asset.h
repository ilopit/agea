#pragma once

#include "packages/root/model/asset.ar.h"

#include "packages/root/model/smart_object.h"

namespace kryga
{
namespace root
{
KRG_ar_class();
class asset : public smart_object
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