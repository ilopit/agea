#pragma once

#include "packages/test/model/test_mesh_component.ar.h"

#include "packages/base/model/components/mesh_component.h"

namespace kryga
{
namespace test
{

KRG_ar_class();
class test_mesh_component : public ::kryga::base::mesh_component
{
    KRG_gen_meta__test_mesh_component();

public:
    KRG_gen_class_meta(test_mesh_component, ::kryga::base::mesh_component);

    KRG_gen_construct_params{};
    KRG_gen_meta_api;
};

}  // namespace test
}  // namespace kryga
