#pragma once

#include "packages/base/model/simple_texture_material.ar.h"

#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture_sample.h"

namespace kryga
{
namespace base
{

KRG_ar_class();
class simple_texture_material : public ::kryga::root::material
{
    KRG_gen_meta__simple_texture_material();

public:
    KRG_gen_class_meta(simple_texture_material, material);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(this_class::construct_params& p);

    const ::kryga::root::texture_sample&
    simple_texture() const
    {
        return m_simple_texture;
    }

protected:
    // clang-format off
    KRG_ar_property("category=Properties",
                     "serializable=true",
                     "property_ser_handler=::kryga::root::property_texture_sample__save",
                     "property_compare_handler=::kryga::root::property_texture_sample__compare",
                     "property_copy_handler=::kryga::root::property_texture_sample__copy",
                     "property_instantiate_handler=::kryga::root::property_texture_sample__instantiate",
                     "property_load_derive_handler=::kryga::root::property_texture_sample__load");
    ::kryga::root::texture_sample m_simple_texture;
    //clang-format off
};

}  // namespace base
}  // namespace kryga
