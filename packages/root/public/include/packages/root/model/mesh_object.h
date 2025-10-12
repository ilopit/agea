#pragma once

#include "packages/root/model/mesh_object.ar.h"

#include "packages/root/model/game_object.h"

namespace agea
{
namespace root
{
class component;

AGEA_ar_class();
class mesh_object : public game_object
{
public:
    // Meta part
    AGEA_gen_class_meta(mesh_object, game_object);
    AGEA_gen_meta_api;

    AGEA_gen_construct_params{};

protected:
    bool
    construct(this_class::construct_params& p);
};

}  // namespace root
}  // namespace agea
