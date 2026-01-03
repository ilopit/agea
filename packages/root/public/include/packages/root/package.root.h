#pragma once

#include <core/package.h>

#include "packages/root/package.ar.h"

#include <ar/ar_defines.h>

namespace kryga::root
{
// clang-format off
KRG_ar_package(model.has_types_overrides = true,
                model.has_properties_overrides = true,
                render.has_overrides = true,
                render.has_resources = false);
class package : public ::kryga::core::package
// clang-format on
{
public:
    package();

    KRG_gen_meta__root_package;
};

}  // namespace kryga::root
