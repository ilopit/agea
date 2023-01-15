#pragma once

#include "simple_material.generated.h"

#include "model/assets/material.h"

namespace agea
{
namespace model
{

AGEA_class();
class simple_material : public material
{
    AGEA_gen_meta__simple_material();

public:
    AGEA_gen_class_meta(simple_material, material);
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
    AGEA_property("category=properties", "access=no", "serializable=true", "gpu_data=MaterialData");
    float m_albedo = 0.2f;

    AGEA_property("category=properties", "access=no", "serializable=true", "gpu_data=MaterialData");
    float m_gamma = 0.4f;

    AGEA_property("category=properties", "access=no", "serializable=true", "gpu_data=MaterialData");
    float m_metallic = 0.6f;

    AGEA_property("category=properties", "access=no", "serializable=true", "gpu_data=MaterialData");
    float m_roughness = 0.8f;
};

}  // namespace model
}  // namespace agea