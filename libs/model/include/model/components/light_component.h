#pragma once

#include "model/light_component.generated.h"

#include "model/components/game_object_component.h"

namespace agea
{
namespace model
{

AGEA_class();
class light_component : public game_object_component
{
    AGEA_gen_meta__light_component();

public:
    AGEA_gen_class_meta(light_component, game_object_component);

    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

protected:
    AGEA_property("category=world", "serializable=true", "hint=x,y,z");
    vec3 m_light;
};

}  // namespace model
}  // namespace agea
