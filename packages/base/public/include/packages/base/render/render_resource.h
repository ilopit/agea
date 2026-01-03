#pragma once

#include <core/package.h>

namespace kryga::base
{
struct root_package_render_resources_loader : ::kryga::core::package_render_custom_resource_builder
{
    virtual bool
    load(::kryga::core::static_package& sp);
};
}  // namespace kryga::base