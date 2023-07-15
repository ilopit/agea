#pragma once

#include "root/directional_light.generated.h"

#include "root/game_object.h"

#include <vector>

namespace agea
{
namespace root
{

AGEA_ar_class();
class directional_light : public game_object
{
    AGEA_gen_meta__directional_light();

public:
    AGEA_gen_class_meta(directional_light, game_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    bool
    construct(construct_params& params);

    virtual void
    on_tick(float) override;
};

}  // namespace root
}  // namespace agea