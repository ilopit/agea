#pragma once

#include "packages/base/model/pbr_material.ar.h"

#include "packages/root/model/assets/material.h"
#include "packages/root/model/assets/texture_slot.h"
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
                    "gpu_texture_slot=1");
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
