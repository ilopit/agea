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

    bool construct(this_class::construct_params& p);
    bool deserialize(json_conteiner& c);
    bool deserialize_finalize(json_conteiner& c);

    bool prepare_for_rendering();

    std::string m_base_effect;
    std::string m_texture_id;

    std::shared_ptr<texture> m_texture;

    ::agea::render::material_data* m_material;
};

}  // namespace model
}  // namespace agea
