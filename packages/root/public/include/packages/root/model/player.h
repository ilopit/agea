#pragma once

#include "packages/root/model/game_object.h"

namespace agea
{
namespace root
{
class player : public game_object
{
public:
    AGEA_gen_class_meta(player, game_object);
    AGEA_gen_construct_params{};

    bool
    construct(construct_params& params)
    {
        base_class::construct(params);

        return true;
    }

private:
};
}  // namespace root
}  // namespace agea
