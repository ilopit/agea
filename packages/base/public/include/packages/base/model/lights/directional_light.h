#pragma once

#include "packages/base/model/directional_light.ar.h"

#include "packages/root/model/game_object.h"

#include <vector>

namespace kryga
{
namespace base
{

KRG_ar_class(render_cmd_builder    = directional_light_component__cmd_builder,
             render_cmd_destroyer = directional_light_component__cmd_destroyer,
             mcp_hint             = "Sun-like light that illuminates the entire scene from a direction — no position falloff");
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