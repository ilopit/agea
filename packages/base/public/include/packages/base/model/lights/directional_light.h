#pragma once

#include "packages/base/model/directional_light.ar.h"

#include "packages/root/model/game_object.h"

#include <vector>

namespace kryga
{
namespace base
{

KRG_ar_class(render_constructor = directional_light_component__render_loader,
              render_destructor = directional_light_component__render_destructor);
class directional_light : public ::kryga::root::game_object
{
    KRG_gen_meta__directional_light();

public:
    KRG_gen_class_meta(directional_light, game_object);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(construct_params& params);

    virtual void
    on_tick(float) override;
};

}  // namespace base
}  // namespace kryga