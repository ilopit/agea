#pragma once

#include <core/package.h>

#include "packages/base/package.ar.h"

#include <ar/ar_defines.h>

namespace agea::base
{
// clang-format off
AGEA_ar_package(model.has_types_overrides = true,
                model.has_properties_overrides = true,
                dependancies = "root",
                render.has_overrides = true,
                render.has_resources = false);
class package : public ::agea::core::package
// clang-format on
{
public:
    package();

    AGEA_gen_meta__base_package;
};

}  // namespace agea::base
