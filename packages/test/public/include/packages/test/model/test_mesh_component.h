#pragma once

#include "packages/test/model/test_mesh_component.ar.h"

#include "packages/base/model/components/mesh_component.h"

namespace agea
{
namespace test
{

AGEA_ar_class();
class test_mesh_component : public ::agea::base::mesh_component
{
    AGEA_gen_meta__test_mesh_component();

public:
    AGEA_gen_class_meta(test_mesh_component, ::agea::base::mesh_component);

    AGEA_gen_construct_params{};
    AGEA_gen_meta_api;
};

}  // namespace test
}  // namespace agea
