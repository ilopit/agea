#pragma once

#include "reflection/module.h"

namespace agea
{
namespace model
{
class model_module : public ::agea::reflection::module
{
public:
    model_module(const ::agea::utils::id& id)
        : ::agea::reflection::module(id)
    {
    }

    virtual bool
    override_reflection_types();

    virtual bool
    init_reflection();

    static model_module&
    instance();
};

}  // namespace model
}  // namespace agea