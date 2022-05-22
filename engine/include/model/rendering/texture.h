#pragma once

#include "model/smart_object.h"

namespace agea
{
namespace render
{
struct texture_data;
}  // namespace render

namespace model
{

class texture : public smart_object
{
public:
    AGEA_gen_class_meta(texture, smart_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    bool
    prepare_for_rendering();

    // Properties
    AGEA_property("category=meta", "serializable=true", "visible=true");
    std::string m_base_color;

    agea::render::texture_data* m_texture;
};

}  // namespace model
}  // namespace agea
