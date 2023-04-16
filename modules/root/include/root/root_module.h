#pragma once

#include "core/reflection/module.h"

namespace agea
{
namespace root
{
class root_module : public ::agea::reflection::module
{
public:
    root_module(const ::agea::utils::id& id)
        : ::agea::reflection::module(id)
    {
    }

    virtual bool
    override_reflection_types();

    virtual bool
    init_reflection();

    static root_module&
    instance();
};

}  // namespace root
}  // namespace agea