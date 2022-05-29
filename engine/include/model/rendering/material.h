#pragma once

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

class material : public smart_object
{
public:
    AGEA_gen_class_meta(material, smart_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    bool
    construct(this_class::construct_params& p);

    bool
    prepare_for_rendering();

    AGEA_property("category=properties", "access=rw", "visible=true", "serializable=true");
    std::string m_base_effect;

    AGEA_property("category=properties", "access=rw", "visible=true", "serializable=true");
    texture* m_texture = nullptr;

    AGEA_property("category=properties", "access=rw", "visible=true", "serializable=false");
    float* m_roughness = nullptr;

    AGEA_property("category=properties", "access=rw", "visible=true", "serializable=false");
    float* m_metallic = nullptr;

    AGEA_property("category=properties", "access=rw", "visible=true", "serializable=false");
    float* m_gamma = nullptr;

    AGEA_property("category=properties", "access=rw", "visible=true", "serializable=false");
    float* m_albedo = nullptr;

    ::agea::render::material_data* m_material = nullptr;
};

}  // namespace model
}  // namespace agea
