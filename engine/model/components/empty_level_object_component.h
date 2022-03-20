#pragma once

#include "model/components/level_object_component.h"

#include "agea_minimal.h"

namespace agea
{
namespace model
{
class empty_level_object_component : public level_object_component
{
public:
    AGEA_gen_class_meta(empty_level_object_component, level_object_component);
    AGEA_gen_construct_params{

    };
    AGEA_gen_meta_api;

    virtual bool prepare_for_rendering() override;
};

}  // namespace model
}  // namespace agea
