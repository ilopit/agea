#pragma once

#include "packages/root/model/game_object.h"

namespace agea
{
namespace base
{
class player : public ::agea::root::game_object
{
public:
    AGEA_gen_class_meta(player, ::agea::root::game_object);
    AGEA_gen_construct_params{};

    bool
    construct(construct_params& params)
    {
        base_class::construct(params);

        return true;
    }

private:
};
}  // namespace base
}  // namespace agea
