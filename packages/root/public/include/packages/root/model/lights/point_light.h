#pragma once

#include "packages/root/model/point_light.ar.h"

#include "packages/root/model/game_object.h"

#include <vector>

namespace agea
{
namespace root
{

AGEA_ar_class(render_constructor = point_light_component__render_loader,
              render_destructor = point_light_component__render_destructor);
class point_light : public game_object
{
    AGEA_gen_meta__point_light();

public:
    AGEA_gen_class_meta(point_light, game_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    bool
    construct(construct_params& params);
};

}  // namespace root
}  // namespace agea