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

    bool serialize(json_conteiner& c);
    bool deserialize(json_conteiner& c);
    bool deserialize_finalize(json_conteiner& c);

    bool prepare_for_rendering();

    std::string m_base_color;

    agea::render::texture_data* m_texture;
};

}  // namespace model
}  // namespace agea
