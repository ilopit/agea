#pragma once

#include <core/package.h>

#include "packages/ui/package.ar.h"

#include <ar/ar_defines.h>

namespace kryga::ui
{
// clang-format off
KRG_ar_package(model.has_types_overrides = false,
                model.has_properties_overrides = false,
                dependancies = "base:root",
                render.has_overrides = true,
                render.has_resources = false);
class package : public ::kryga::core::package
// clang-format on
{
public:
    package();

    KRG_gen_meta__ui_package;
};

}  // namespace kryga::ui
