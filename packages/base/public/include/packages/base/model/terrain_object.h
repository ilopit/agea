#pragma once

#include "packages/base/model/terrain_object.ar.h"

#include "packages/root/model/game_object.h"

namespace kryga
{
namespace base
{

// clang-format off
KRG_ar_class(
    mcp_hint = "Renderable terrain object — owns a terrain_component that generates a heightfield "
               "mesh and references a terrain material"
);
class terrain_object : public ::kryga::root::game_object
// clang-format on
{
public:
    KRG_gen_class_meta(terrain_object, game_object);
    KRG_gen_meta_api;

    KRG_gen_construct_params{};

protected:
    bool
    construct(this_class::construct_params& p);
};

}  // namespace base
}  // namespace kryga
