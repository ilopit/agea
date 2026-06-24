#pragma once

#include "packages/root/model/destructible_mesh_object.ar.h"

#include "packages/root/model/game_object.h"

namespace kryga
{
namespace root
{

KRG_ar_class();
class destructible_mesh_object : public ::kryga::root::game_object
{
public:
    KRG_gen_class_meta(destructible_mesh_object, game_object);
    KRG_gen_meta_api;

    KRG_gen_construct_params{};

protected:
    bool
    construct(this_class::construct_params& p);
};

}  // namespace root
}  // namespace kryga
