#pragma once

#include "packages/base/model/spot_light.ar.h"

#include "packages/root/model/game_object.h"

#include <vector>

namespace kryga
{
namespace base
{

KRG_ar_class(render_constructor = spot_light_component__render_loader,
              render_destructor = spot_light_component__render_destructor);
class spot_light : public ::kryga::root::game_object
{
    KRG_gen_meta__spot_light();

public:
    KRG_gen_class_meta(spot_light, game_object);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(construct_params& params);

    virtual void
    on_tick(float) override;
};

}  // namespace base
}  // namespace kryga