#pragma once

#include "model/level_object.h"

namespace agea
{
namespace model
{
class player : public level_object
{
public:
    AGEA_gen_class_meta(player, level_object);
    AGEA_gen_construct_params{};

    bool
    construct(construct_params& params)
    {
        base_class::construct(params);

        return true;
    }

private:
};
}  // namespace model
}  // namespace agea
