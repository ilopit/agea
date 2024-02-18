#pragma once

#include "packages/root/simple_texture_material.generated.h"

#include "packages/root/assets/material.h"
#include "packages/root/assets/texture_sample.h"

namespace agea
{
namespace root
{

AGEA_ar_class();
class simple_texture_material : public material
{
    AGEA_gen_meta__simple_texture_material();

public:
    AGEA_gen_class_meta(simple_texture_material, material);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    bool
    construct(this_class::construct_params& p);

protected:
    AGEA_ar_property("category=Properties",
                     "serializable=true",
                     "property_des_handler=custom::texture_sample_deserialize",
                     "property_ser_handler=custom::texture_sample_serialize",
                     "property_prototype_handler=custom::texture_sample_prototype",
                     "property_compare_handler=custom::texture_sample_compare",
                     "property_copy_handler=custom::texture_sample_copy");
    texture_sample m_simple_texture;
};

}  // namespace root
}  // namespace agea
