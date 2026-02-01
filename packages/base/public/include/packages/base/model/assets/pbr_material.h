#pragma once

#include "packages/base/model/pbr_material.ar.h"

#include "packages/root/model/assets/material.h"
#include "packages/root/model/core_types/vec3.h"

namespace kryga
{
namespace base
{
KRG_ar_class();
class pbr_material : public ::kryga::root::material
{
    KRG_gen_meta__pbr_material();

public:
    KRG_gen_class_meta(pbr_material, ::kryga::root::material);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(this_class::construct_params& p)
    {
        return true;
    };

    KRG_ar_property(
        "category=Properties",
        "serializable=true",
        "property_ser_handler=::kryga::root::property_texture_slot__save",
        "property_compare_handler=::kryga::root::property_texture_slot__compare",
        "property_instantiate_handler=::kryga::root::property_texture_slot__instantiate",
        "property_load_derive_handler=::kryga::root::property_texture_slot__load",
        "property_copy_handler=::kryga::root::property_texture_slot__copy");
    ::kryga::root::texture_slot m_diffuse_txt;

    KRG_ar_property(
        "category=Properties",
        "serializable=true",
        "property_ser_handler=::kryga::root::property_texture_slot__save",
        "property_compare_handler=::kryga::root::property_texture_slot__compare",
        "property_instantiate_handler=::kryga::root::property_texture_slot__instantiate",
        "property_load_derive_handler=::kryga::root::property_texture_slot__load",
        "property_copy_handler=::kryga::root::property_texture_slot__copy");
    ::kryga::root::texture_slot m_specular_txt;

    KRG_ar_property("category=Properties",
                     "serializable=true",
                     "invalidates=render",
                     "check=not_same",
                     "access=all",
                     "gpu_data=MaterialData",
                     "default=true");
    ::kryga::root::vec3 m_ambient = {.2f, .2f, .2f};

    KRG_ar_property("category=Properties",
                     "serializable=true",
                     "access=all",
                     "gpu_data=MaterialData",
                     "default=true");
    ::kryga::root::vec3 m_diffuse = {.2f, .2f, .2f};

    KRG_ar_property("category=Properties",
                     "serializable=true",
                     "access=all",
                     "gpu_data=MaterialData",
                     "default=true");
    ::kryga::root::vec3 m_specular = 0.5f;

    KRG_ar_property("category=Properties",
                     "serializable=true",
                     "access=no",
                     "default=true",
                     "gpu_data=MaterialData");
    float m_shininess = 64.0f;
};

}  // namespace base
}  // namespace kryga
