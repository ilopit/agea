#pragma once

#include "model/game_object.h"
#include "mesh_object.generated.h"

namespace agea
{
namespace model
{
class component;

AGEA_class();
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

}  // namespace model
}  // namespace agea
