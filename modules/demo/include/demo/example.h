
#pragma once

#include "demo/example.generated.h"

#include "model/model_minimal.h"
#include "root/core_types/vec3.h"
#include "root/game_object.h"

namespace demo
{
AGEA_ar_class();
class example : public ::agea::root::game_object
{
    AGEA_gen_meta__example();

public:
    // Meta part
    AGEA_gen_class_meta(example, ::agea::root::game_object);
    AGEA_gen_meta_api;
    AGEA_gen_construct_params{

    };

    bool construct(construct_params& params);

};

}  // namespace demo
