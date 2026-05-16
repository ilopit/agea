#pragma once

#include "packages/base/model/toon_material.ar.h"

#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture_slot.h"

namespace kryga
{
namespace base
{

KRG_ar_class();
class toon_material : public ::kryga::root::material
{
    KRG_gen_meta__toon_material();

public:
    KRG_gen_class_meta(toon_material, material);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(this_class::construct_params& p)
    {
        return true;
    }

    const ::kryga::root::texture_slot&
    diffuse_txt() const
    {
        return m_diffuse_txt;
    }

protected:
    KRG_ar_property("category=Properties",
                    "serializable=true",
                    "invalidates=render",
                    "access=all",
                    "gpu_texture_slot=0");
    ::kryga::root::texture_slot m_diffuse_txt;

    KRG_ar_property("category=Properties",
                    "serializable=true",
                    "invalidates=render",
                    "access=all",
                    "gpu_data=MaterialData",
                    "default=true");
    float m_band_count = 4.0f;

    KRG_ar_property("category=Properties",
                    "serializable=true",
                    "invalidates=render",
                    "access=all",
                    "gpu_data=MaterialData",
                    "default=true");
    float m_specular_strength = 1.0f;

    KRG_ar_property("category=Properties",
                    "serializable=true",
                    "invalidates=render",
                    "access=all",
                    "gpu_data=MaterialData",
                    "default=true");
    float m_shininess = 32.0f;
};

}  // namespace base
}  // namespace kryga
