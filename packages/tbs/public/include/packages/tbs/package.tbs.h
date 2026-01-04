#pragma once

#include <core/package.h>

#include "packages/tbs/package.ar.h"

#include <ar/ar_defines.h>

namespace kryga::tbs
{
// clang-format off
KRG_ar_package(model.has_types_overrides = false,
                model.has_properties_overrides = false,
                dependancies = "base:root",
                render.has_overrides = false,
                render.has_resources = false);
class package : public ::kryga::core::package
// clang-format on
{
public:
    package();

    KRG_gen_meta__tbs_package;
};

}  // namespace kryga::tbs
