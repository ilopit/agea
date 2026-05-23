#pragma once

#include "packages/base/model/spot_light.ar.h"

#include "packages/root/model/game_object.h"

#include <vector>

namespace kryga
{
namespace base
{

// clang-format off
KRG_ar_class(
    render_cmd_builder   = spot_light_component__cmd_builder,
    render_cmd_destroyer = spot_light_component__cmd_destroyer,
    mcp_hint             = "Cone-shaped light with direction / distance falloff and inner/outer "
                           "cone angles"
);
class spot_light : public ::kryga::root::game_object
// clang-format on
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