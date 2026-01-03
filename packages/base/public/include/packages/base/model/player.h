#pragma once

#include "packages/root/model/game_object.h"

namespace kryga
{
namespace base
{
class player : public ::kryga::root::game_object
{
public:
    KRG_gen_class_meta(player, ::kryga::root::game_object);
    KRG_gen_construct_params{};

    bool
    construct(construct_params& params)
    {
        base_class::construct(params);

        return true;
    }

private:
};
}  // namespace base
}  // namespace kryga
