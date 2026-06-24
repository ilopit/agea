#pragma once

#include "packages/test/model/test_mesh_component.ar.h"

#include "packages/root/model/components/mesh_component.h"

namespace kryga
{
namespace test
{

// clang-format off
KRG_ar_class(
    mcp_hint = "Test mesh_component — used in automated tests for mesh rendering validation"
);
class test_mesh_component : public ::kryga::root::mesh_component
// clang-format on
{
    KRG_gen_meta__test_mesh_component();

public:
    KRG_gen_class_meta(test_mesh_component, ::kryga::root::mesh_component);

    KRG_gen_construct_params{};
    KRG_gen_meta_api;
};

}  // namespace test
}  // namespace kryga
