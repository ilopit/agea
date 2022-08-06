#pragma once

#include "texture.generated.h"

#include "model/smart_object.h"

namespace agea
{
namespace render
{
struct texture_data;
}  // namespace render

namespace model
{

AGEA_class();
class texture : public smart_object
{
    AGEA_gen_meta__texture();

public:
    AGEA_gen_class_meta(texture, smart_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_architype_api(texture);

    bool
    prepare_for_rendering();

protected:
    // Properties
    AGEA_property("category=meta", "serializable=true");
    std::string m_base_color;

    agea::render::texture_data* m_texture = nullptr;
};

}  // namespace model
}  // namespace agea
