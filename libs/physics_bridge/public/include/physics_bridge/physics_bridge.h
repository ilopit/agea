#pragma once

#include "physics_bridge/physics_command.h"

#include <core/model_minimal.h>

#include <new>
#include <utility>

namespace kryga
{
namespace root
{
class smart_object;
class game_object_component;
}  // namespace root

// physics_bridge is the model→physics counterpart of render_translator: a producer
// that turns model objects into physics commands, dispatched through per-type
// reflection handlers (physics_cmd_builder / destroyer / transform).
//
// Unlike render_translator it holds no dependency graph and does NOT touch the
// smart_object render-state machine — render owns that lifecycle. Physics "built"
// state is tracked per component (e.g. terrain's static_body_handle validity), so
// the handlers are idempotent on their own.
class physics_bridge
{
public:
    kryga::result_code
    physics_cmd_build(root::smart_object& obj, bool sub_objects);

    kryga::result_code
    physics_cmd_destroy(root::smart_object& obj, bool sub_objects);

    kryga::result_code
    physics_cmd_transform(root::game_object_component& source);

    // Raw allocation from the physics input queue's arena (implemented in .cpp).
    static void*
    alloc_cmd_raw(size_t size, size_t align);

    template <typename T, typename... Args>
    T*
    alloc_cmd(Args&&... args);

    void
    enqueue_cmd(physics_cmd::physics_command_base* cmd);
};

template <typename T, typename... Args>
T*
physics_bridge::alloc_cmd(Args&&... args)
{
    void* mem = alloc_cmd_raw(sizeof(T), alignof(T));
    return new (mem) T(std::forward<Args>(args)...);
}

}  // namespace kryga
