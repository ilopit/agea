#pragma once

#include "solid_color_material.generated.h"

#include "model/assets/material.h"
#include "model/assets/texture_sample.h"

namespace agea
{
namespace model
{

AGEA_class();
class solid_color_material : public material
{
    AGEA_gen_meta__solid_color_material();

public:
    AGEA_gen_class_meta(solid_color_material, material);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

protected:
    AGEA_property("category=properties",
                  "access=no",
                  "serializable=true",
                  "gpu_data=MaterialData",
                  "default=true");
    vec3 m_ambient = {1.0f, 0.5f, 0.31f};

    AGEA_property("category=properties",
                  "access=no",
                  "serializable=true",
                  "gpu_data=MaterialData",
                  "default=true");
    vec3 m_diffuse = {1.0f, 0.5f, 0.31f};

    AGEA_property("category=properties",
                  "access=no",
                  "serializable=true",
                  "gpu_data=MaterialData",
                  "default=true");
    vec3 m_specular = 0.5f;

    AGEA_property("category=properties",
                  "access=no",
                  "serializable=true",
                  "gpu_data=MaterialData",
                  "default=true");
    float m_shininess = 32.0f;
};

}  // namespace model
}  // namespace agea
