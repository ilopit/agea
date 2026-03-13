#pragma once

#include "packages/base/model/point_light.ar.h"

#include "packages/root/model/game_object.h"

#include <vector>

namespace kryga
{
namespace base
{

KRG_ar_class(render_cmd_builder   = point_light_component__cmd_builder,
              render_cmd_destroyer = point_light_component__cmd_destroyer);
class point_light : public ::kryga::root::game_object
{
    KRG_gen_meta__point_light();

public:
    KRG_gen_class_meta(point_light, ::kryga::root::game_object);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(construct_params& params);
};

}  // namespace base
}  // namespace kryga