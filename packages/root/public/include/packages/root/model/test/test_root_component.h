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

KRG_ar_class("architype=component");
class test_root_component : public component
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

    // -- POD: instantiate (default) --
    // On instantiate: deep-copied via value copy chain.
    // Instance gets its own copy of the value.
    KRG_ar_property("category=Test", "serializable=true");
    float m_pod_instantiate = 0.0f;

    // -- POD: share --
    // On instantiate: raw memcpy of sizeof(void*) bytes.
    // For PODs this is effectively the same as copy, but tests the share path.
    KRG_ar_property("category=Test", "serializable=true", "instantiate=share");
    float m_pod_share = 0.0f;

    // -- smart_object*: instantiate (default) --
    // On instantiate: goes through smart_obj__instantiate.
    // If source is readonly → shared (smart_obj__instantiate checks readonly).
    // If source is mutable → creates a new instance object.
    KRG_ar_property("category=Test", "serializable=true");
    smart_object* m_obj_instantiate = nullptr;

    // -- smart_object*: share --
    // On instantiate: pointer memcpy, always shares regardless of readonly flag.
    // Instance and proto point to the exact same object.
    KRG_ar_property("category=Test", "serializable=true", "instantiate=share");
    smart_object* m_obj_share = nullptr;
};

}  // namespace kryga::root
