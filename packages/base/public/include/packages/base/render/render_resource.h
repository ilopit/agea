#pragma once

#include <core/package.h>

namespace agea::base
{
struct root_package_render_resources_loader : ::agea::core::package_render_custom_resource_builder
{
    virtual bool
    load(::agea::core::static_package& sp);
};
}  // namespace agea::base