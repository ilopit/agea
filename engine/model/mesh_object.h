#pragma once

#include "agea_minimal.h"

namespace agea
{
namespace model
{
class component;

class mesh_object : public level_object
{
public:
    // Meta part
    AGEA_gen_class_meta(mesh_object, level_object);
    AGEA_gen_meta_api;

    AGEA_gen_construct_params{};

    bool clone(this_class& src);

protected:
    bool construct(this_class::construct_params& p);
    bool deserialize(json_conteiner& c);
    bool deserialize_finalize(json_conteiner& c);
};

}  // namespace model
}  // namespace agea
