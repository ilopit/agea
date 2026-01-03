#pragma once

#include "packages/base/model/point_light.ar.h"

#include "packages/root/model/game_object.h"

#include <vector>

namespace kryga
{
namespace base
{

KRG_ar_class(render_constructor = point_light_component__render_loader,
              render_destructor = point_light_component__render_destructor);
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