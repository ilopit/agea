#pragma once

#include "packages/test/model/test_mesh_object.ar.h"

#include "packages/base/model/mesh_object.h"

#include <vector>

namespace kryga
{
namespace test
{

KRG_ar_class();
class test_mesh_object : public ::kryga::base::mesh_object
{
    KRG_gen_meta__test_mesh_object();

public:
    KRG_gen_class_meta(test_mesh_object, ::kryga::base::mesh_object);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(construct_params& params);
};

}  // namespace test
}  // namespace kryga