
#pragma once

#define AGEA_root_module_render_bridge_included

#include "core/package.h"

namespace agea
{

namespace reflection
{
class reflection_type_registry;
}

namespace root
{
class package : public ::agea::core::static_package
{
public:
    package();

    static package&
    instance();

    bool
    init_reflection();

    bool
    finilize_objects();

    virtual bool
    override_reflection_types();
};

}  // namespace root
}  // namespace agea