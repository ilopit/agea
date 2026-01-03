#pragma once

#include "packages/base/model/mesh_object.ar.h"

#include "packages/root/model/game_object.h"

namespace kryga
{
namespace base
{
class component;

KRG_ar_class();
class mesh_object : public ::kryga::root::game_object
{
public:
    // Meta part
    KRG_gen_class_meta(mesh_object, game_object);
    KRG_gen_meta_api;

    KRG_gen_construct_params{};

protected:
    bool
    construct(this_class::construct_params& p);
};

}  // namespace base
}  // namespace kryga
