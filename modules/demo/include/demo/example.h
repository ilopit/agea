
#pragma once

#include "demo/example.generated.h"

#include "model/model_minimal.h"
#include "model/core_types/vec3.h"
#include "model/game_object.h"

namespace demo
{
AGEA_class();
class example : public ::agea::model::game_object
{
    AGEA_gen_meta__example();

public:
    // Meta part
    AGEA_gen_class_meta(example, ::agea::model::game_object);
    AGEA_gen_meta_api;
    AGEA_gen_construct_params{

    };

    bool construct(construct_params& params);

};

}  // namespace demo
