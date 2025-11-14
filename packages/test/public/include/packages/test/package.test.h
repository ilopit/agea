#pragma once

#include <core/package.h>

#include "packages/test/package.ar.h"

#include <ar/ar_defines.h>

namespace agea::test
{
// clang-format off
AGEA_ar_package(model.has_types_overrides = false,
                model.has_properties_overrides = false,
                dependancies = "base:root",
                render.has_overrides = false,
                render.has_resources = false);
class package : public ::agea::core::static_package
// clang-format on
{
public:
    package();

    AGEA_gen_meta__test_package;
};

}  // namespace agea::root
