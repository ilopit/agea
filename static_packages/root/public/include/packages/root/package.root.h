#pragma once

#include "core/package.h"

namespace agea::root
{
class package : public ::agea::core::static_package
{
public:
    package();

    AGEA_ar_package_types_loader;

    AGEA_ar_package_custom_types_loader;

    AGEA_ar_package_render_types_loader;

    AGEA_ar_package_render_data_loader;

    AGEA_ar_package_object_builder;

    static package&
    instance();
};

}  // namespace agea::root
