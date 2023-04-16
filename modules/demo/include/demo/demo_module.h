
#pragma once

#include "core/reflection/module.h"

namespace demo
{
class demo_module : public ::agea::reflection::module
{
public:
    demo_module(const ::agea::utils::id& id)
        : ::agea::reflection::module(id)
    {
    }

    virtual bool
    override_reflection_types()
    {
        return true;
    }

    virtual bool
    init_reflection();

    static demo_module&
    instance();
};

}  // namespace demo
