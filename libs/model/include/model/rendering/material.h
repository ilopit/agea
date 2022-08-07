#pragma once

#include "material.generated.h"
#include "model/smart_object.h"

namespace agea
{
namespace render
{
struct material_data;
}  // namespace render
namespace model
{
class texture;

AGEA_class();
class material : public smart_object
{
    AGEA_gen_meta__material();

public:
    AGEA_gen_class_meta(material, smart_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_architype_api(material);

    bool
    construct(this_class::construct_params& p);

    bool
    prepare_for_rendering();

    ::agea::render::material_data*
    get_material_data() const
    {
        return m_material_data;
    }

protected:
    AGEA_property("category=properties", "access=cpp_only", "serializable=true");
    utils::id m_base_effect;

    AGEA_property("category=properties", "access=cpp_only", "serializable=true");
    std::string m_config_path;

    AGEA_property("category=properties", "access=cpp_only", "serializable=true");
    texture* m_base_texture = nullptr;

    AGEA_property("category=properties", "access=all", "serializable=false", "ref=true");
    float* m_roughness = nullptr;

    AGEA_property("category=properties", "access=all", "serializable=false", "ref=true");
    float* m_metallic = nullptr;

    AGEA_property("category=properties", "access=all", "serializable=false", "ref=true");
    float* m_gamma = nullptr;

    AGEA_property("category=properties", "access=all", "serializable=false", "ref=true");
    float* m_albedo = nullptr;

    ::agea::render::material_data* m_material_data = nullptr;
};

}  // namespace model
}  // namespace agea
