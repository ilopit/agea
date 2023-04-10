#pragma once

#include "root/asset.generated.h"

#include "root/smart_object.h"

namespace agea
{
namespace root
{
AGEA_ar_class();
class asset : public smart_object
{
    AGEA_gen_meta__asset();

public:
    AGEA_gen_class_meta(asset, smart_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    void
    mark_render_dirty();
};
}  // namespace root
}  // namespace agea