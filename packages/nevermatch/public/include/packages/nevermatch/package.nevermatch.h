#pragma once

#include <core/package.h>

#include "packages/nevermatch/package.ar.h"

#include <ar/ar_defines.h>

namespace kryga::nevermatch
{
// clang-format off
KRG_ar_package(model.has_types_overrides = false,
                model.has_properties_overrides = false,
                dependancies = "base:root",
                render.has_overrides = false,
                render.has_resources = false,
                included = true);
class package : public ::kryga::core::package
// clang-format on
{
public:
    package();

    KRG_gen_meta__nevermatch_package;
};

}  // namespace kryga::nevermatch
