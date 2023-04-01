#pragma once

#include "model/simple_texture_material.generated.h"

#include "model/assets/material.h"
#include "model/assets/texture_sample.h"

namespace agea
{
namespace model
{

AGEA_class();
class simple_texture_material : public material
{
    AGEA_gen_meta__simple_texture_material();

public:
    AGEA_gen_class_meta(simple_texture_material, material);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    bool
    construct(this_class::construct_params& p);

    AGEA_function("category=world");
    void
    set_albedo(float v)
    {
        m_albedo = v;
        mark_render_dirty();
    }

    AGEA_function("category=world");
    float
    get_albedo()
    {
        return m_albedo;
    }

    AGEA_function("category=world");
    void
    set_gamma(float v)
    {
        m_gamma = v;
        mark_render_dirty();
    }

    AGEA_function("category=world");
    float
    get_gamma()
    {
        return m_gamma;
    }

    AGEA_function("category=world");
    void
    set_metallic(float v)
    {
        m_metallic = v;
        mark_render_dirty();
    }

    AGEA_function("category=world");
    float
    get_metallic()
    {
        return m_metallic;
    }

    AGEA_function("category=world");
    void
    set_roughness(float v)
    {
        m_roughness = v;
        mark_render_dirty();
    }

    AGEA_function("category=world");
    float
    get_roughness()
    {
        return m_roughness;
    }

protected:
    AGEA_property("category=meta",
                  "serializable=true",
                  "property_des_handler=custom::texture_sample_deserialize",
                  "property_ser_handler=custom::texture_sample_serialize",
                  "property_prototype_handler=custom::texture_sample_prototype",
                  "property_compare_handler=custom::texture_sample_compare",
                  "property_copy_handler=custom::texture_sample_copy");
    texture_sample m_simple_texture;

    AGEA_property("category=properties",
                  "access=no",
                  "serializable=true",
                  "gpu_data=MaterialData",
                  "default=true");
    float m_albedo = 0.2f;

    AGEA_property("category=properties",
                  "access=no",
                  "serializable=true",
                  "gpu_data=MaterialData",
                  "default=true");
    float m_gamma = 0.4f;

    AGEA_property("category=properties",
                  "access=no",
                  "serializable=true",
                  "gpu_data=MaterialData",
                  "default=true");
    float m_metallic = 0.6f;

    AGEA_property("category=properties",
                  "access=no",
                  "serializable=true",
                  "gpu_data=MaterialData",
                  "default=true");
    float m_roughness = 0.8f;
};

}  // namespace model
}  // namespace agea
