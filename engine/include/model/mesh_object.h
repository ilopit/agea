#pragma once

#include "core/agea_minimal.h"

#include "model/game_object.h"

namespace agea
{
namespace model
{
class component;

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
