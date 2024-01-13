
#pragma once

#define AGEA_root_module_render_bridge_included

#include "core/package.h"

namespace agea
{
namespace root
{
class package : public ::agea::core::package
{
public:
    package(const ::agea::utils::id& id);

    static package&
    instance();

    bool
    init_reflection();

    virtual bool
    override_reflection_types();
};

}  // namespace root
}  // namespace agea