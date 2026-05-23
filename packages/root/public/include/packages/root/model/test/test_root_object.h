#pragma once

// Test game_object for validating instantiate_mode behavior.
//
// This object exercises all combinations of instantiate_mode × property type:
//
//   ┌──────────────────┬──────────────────────┬──────────────────────────────┐
//   │ Property         │ inst_mode            │ Expected on instantiate      │
//   ├──────────────────┼──────────────────────┼──────────────────────────────┤
//   │ m_pod_instantiate│ instantiate (default)│ value deep-copied            │
//   │ m_pod_share      │ share                │ memcpy (same bits)           │
//   │ m_obj_instantiate│ instantiate (default)│ smart_obj__instantiate:      │
//   │                  │                      │   readonly → share ptr       │
//   │                  │                      │   mutable  → new instance    │
//   │ m_obj_share      │ share                │ pointer copied as-is         │
//   └──────────────────┴──────────────────────┴──────────────────────────────┘
//
// test_root_component carries the same property layout, so we can verify
// the behavior propagates through game_object → component instantiation.
//
// In construct(), we spawn a test_root_component as a subcomponent.
// Tests should:
//   1. construct_obj(is_proto=true) to create a proto test_root_object
//   2. Set the property values on the proto (and its component)
//   3. instantiate_obj() from the proto
//   4. Verify each property on the instance according to the table above

#include "packages/root/model/test_root_object.ar.h"

#include "packages/root/model/game_object.h"
#include "packages/root/model/test/test_root_component.h"

namespace kryga::root
{

// clang-format off
KRG_ar_class(
    "architype=game_object",
    mcp_hint = "Test game_object with POD and object-pointer properties for "
               "instantiation/serialization testing"
);
class test_root_object : public game_object
// clang-format on
{
    KRG_gen_meta__test_root_object();

public:
    KRG_gen_class_meta(test_root_object, game_object);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(construct_params& params);

    bool
    construct_default(construct_params& params);

    // -- POD: instantiate (default) --
    // Value deep-copied on instantiate. Instance gets independent copy.
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

    test_root_component*
    add_test_component(component* parent, const utils::id& name);

    test_root_component*
    get_test_component();
};

}  // namespace kryga::root
