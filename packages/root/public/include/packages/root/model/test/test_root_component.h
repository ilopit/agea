#pragma once

// Test component for validating instantiate_mode behavior on properties.
//
// Properties mirror test_root_object's layout so we can verify that the
// instantiation rules propagate correctly through the game_object → component
// hierarchy:
//
//   instantiate (default):
//     - POD:           deep-copied via copy_handler / instantiate_handler chain
//     - smart_object*: goes through smart_obj__instantiate (creates instance or
//                      shares if readonly)
//
//   share:
//     - POD:           raw memcpy of sizeof(void*) — effectively same bits
//     - smart_object*: pointer copied as-is, both proto and instance point to
//                      the same object (no clone, no instantiation)

#include "packages/root/model/test_root_component.ar.h"

#include "packages/root/model/components/component.h"

namespace kryga::root
{

// clang-format off
KRG_ar_class(
    "architype=component",
    mcp_hint = "Test component with POD and object-pointer properties for "
               "instantiation/serialization testing"
);
class test_root_component : public component
// clang-format on
{
    KRG_gen_meta__test_root_component();

public:
    KRG_gen_class_meta(test_root_component, component);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(construct_params& params);

    bool
    construct_default(construct_params& params);

    // clang-format off
    KRG_ar_property(
        category     = "Test",
        serializable = true,
        mcp_hint     = "test float — deep-copied on instantiate"
    );
    float m_pod_instantiate = 0.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Test",
        serializable = true,
        instantiate  = share,
        mcp_hint     = "test float — shared on instantiate"
    );
    float m_pod_share = 0.0f;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Test",
        serializable = true,
        mcp_hint     = "test object ref — deep-copied on instantiate"
    );
    smart_object* m_obj_instantiate = nullptr;
    // clang-format on

    // clang-format off
    KRG_ar_property(
        category     = "Test",
        serializable = true,
        instantiate  = share,
        mcp_hint     = "test object ref — always shared"
    );
    smart_object* m_obj_share = nullptr;
    // clang-format on
};

}  // namespace kryga::root
