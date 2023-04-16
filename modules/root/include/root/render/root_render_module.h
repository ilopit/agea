#pragma once

#include "root/root_module.h"

namespace agea
{
namespace root
{
class root_render_module : public root_module
{
public:
    root_render_module(const ::agea::utils::id& id)
        : root_module(id)
    {
    }

    virtual bool
    override_reflection_types();

    static root_render_module&
    instance();
};

}  // namespace root
}  // namespace agea