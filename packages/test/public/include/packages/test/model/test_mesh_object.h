#pragma once

#include "packages/test/model/test_mesh_object.ar.h"

#include "packages/base/model/mesh_object.h"

#include <vector>

namespace agea
{
namespace test
{

AGEA_ar_class();
class test_mesh_object : public ::agea::base::mesh_object
{
    AGEA_gen_meta__test_mesh_object();

public:
    AGEA_gen_class_meta(test_mesh_object, ::agea::base::mesh_object);
    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;

    bool
    construct(construct_params& params);
};

}  // namespace test
}  // namespace agea