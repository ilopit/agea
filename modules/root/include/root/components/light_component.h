#pragma once

#include "root/light_component.generated.h"

#include "root/components/game_object_component.h"

namespace agea
{
namespace root
{

AGEA_ar_class();
class light_component : public game_object_component
{
    AGEA_gen_meta__light_component();

public:
    AGEA_gen_class_meta(light_component, game_object_component);

    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

protected:
    AGEA_ar_property("category=world", "serializable=true", "hint=x,y,z");
    vec3 m_light;
};

}  // namespace root
}  // namespace agea
