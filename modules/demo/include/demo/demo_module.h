
#pragma once

#include "model/reflection/module.h"

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

    virtual bool init_reflection();

    demo_module& instance();
};

}  // namespace demo
