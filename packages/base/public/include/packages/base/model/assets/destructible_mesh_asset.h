#pragma once

#include "packages/base/model/destructible_mesh_asset.ar.h"

#include "packages/root/model/assets/asset.h"

namespace kryga
{
namespace root
{
class mesh;
class material;
}  // namespace root

namespace base
{
// Destructible mesh asset — UE-style offline fracture description.
//
// Holds a reference to a source mesh + material and the parameters the
// fracture pipeline uses to split the source into Voronoi chunks. Chunk
// geometry is generated lazily at load time by the render command builder
// from these parameters; a future PR can move fracturing into the asset
// importer and serialize the chunks directly on this asset.
KRG_ar_class();
class destructible_mesh_asset : public ::kryga::root::asset
{
    KRG_gen_meta__destructible_mesh_asset();

public:
    KRG_gen_class_meta(destructible_mesh_asset, ::kryga::root::asset);
    KRG_gen_construct_params
    {
        ::kryga::root::mesh* source_mesh = nullptr;
        ::kryga::root::material* material_handle = nullptr;
    };
    KRG_gen_meta_api;

    bool
    construct(this_class::construct_params& p);

protected:
    // clang-format off
    KRG_ar_property(
        "category=assets",
        "serializable=true",
        "check=not_same",
        "invalidates=render",
        "access=all",
        "default=true"
    );
    ::kryga::root::mesh* m_source_mesh = nullptr;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=assets",
        "serializable=true",
        "check=not_same",
        "invalidates=render",
        "access=all",
        "default=true"
    );
    ::kryga::root::material* m_material = nullptr;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=fracture",
        "serializable=true",
        "check=not_same",
        "invalidates=render",
        "access=all",
        "default=true"
    );
    uint32_t m_cell_count = 8;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=fracture",
        "serializable=true",
        "check=not_same",
        "invalidates=render",
        "access=all",
        "default=true"
    );
    uint32_t m_fracture_seed = 0;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=fracture",
        "serializable=true",
        "check=not_same",
        "access=all",
        "default=true"
    );
    float m_damage_threshold = 100.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=fracture",
        "serializable=true",
        "check=not_same",
        "access=all",
        "default=true"
    );
    float m_lifetime = 3.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        "category=fracture",
        "serializable=true",
        "check=not_same",
        "access=all",
        "default=true"
    );
    float m_explosion_strength = 8.0f;
    // clang-format on
};

}  // namespace base
}  // namespace kryga
