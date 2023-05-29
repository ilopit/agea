#pragma once

#define AGEA_root_module_render_bridge_included

#include "root/root_module.h"

namespace agea
{
namespace root
{
class root_module_render_bridge : public reflection::module
{
public:
    root_module_render_bridge(const ::agea::utils::id& id)
        : reflection::module(id)
    {
    }

    virtual bool
    override_reflection_types() override;

    static root_module_render_bridge&
    instance();
};

}  // namespace root
}  // namespace agea