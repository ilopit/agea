#pragma once

#include <core/package.h>

#include "packages/root/package.ar.h"

#include <ar/ar_defines.h>

namespace agea::root
{
// clang-format off
AGEA_ar_package(model.has_types_overrides = true,
                model.has_properties_overrides = true,
                render.has_overrides = true,
                render.has_resources = true);
class package : public ::agea::core::static_package
// clang-format on
{
public:
    package();

    AGEA_gen_meta__root_package;
};

}  // namespace agea::root
