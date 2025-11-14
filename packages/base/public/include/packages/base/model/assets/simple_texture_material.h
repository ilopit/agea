#pragma once

#include "packages/base/model/simple_texture_material.ar.h"

#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture_sample.h"

namespace agea
{
namespace base
{

AGEA_ar_class();
class simple_texture_material : public ::agea::root::material
{
    AGEA_gen_meta__simple_texture_material();

public:
    AGEA_gen_class_meta(simple_texture_material, material);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    bool
    construct(this_class::construct_params& p);

    const ::agea::root::texture_sample&
    simple_texture() const
    {
        return m_simple_texture;
    }

protected:
    // clang-format off
    AGEA_ar_property("category=Properties",
                     "serializable=true",
                     "property_des_handler=::agea::root::texture_sample_deserialize",
                     "property_ser_handler=::agea::root::texture_sample_serialize",
                     "property_compare_handler=::agea::root::texture_sample_compare",
                     "property_copy_handler=::agea::root::texture_sample_copy",
                     "property_load_derive_handler=::agea::root::texture_load_derive");
    ::agea::root::texture_sample m_simple_texture;
    //clang-format off
};

}  // namespace base
}  // namespace agea
