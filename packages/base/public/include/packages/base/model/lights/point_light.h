#pragma once

#include "packages/base/model/point_light.ar.h"

#include "packages/root/model/game_object.h"

#include <vector>

namespace agea
{
namespace base
{

AGEA_ar_class(render_constructor = point_light_component__render_loader,
              render_destructor = point_light_component__render_destructor);
class point_light : public ::agea::root::game_object
{
    AGEA_gen_meta__point_light();

public:
    AGEA_gen_class_meta(point_light, ::agea::root::game_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    bool
    construct(construct_params& params);
};

}  // namespace base
}  // namespace agea