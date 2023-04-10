#pragma once

#include "root/point_light.generated.h"

#include "root/game_object.h"

#include <vector>

namespace agea
{
namespace root
{

AGEA_ar_class();
class point_light : public game_object
{
    AGEA_gen_meta__point_light();

public:
    AGEA_gen_class_meta(point_light, game_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;
};

}  // namespace root
}  // namespace agea