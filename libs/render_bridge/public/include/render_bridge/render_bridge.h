#pragma once

#include "render_bridge/render_dependency.h"
#include "render_bridge/render_command.h"

#include <utils/id.h>
#include <utils/check.h>

#include <core/model_minimal.h>

#include <new>

namespace kryga
{
namespace root
{
class smart_object;
class game_object_component;
}  // namespace root

// render_bridge owns the stateful render-command lifecycle: it builds/destroys/
// transforms render commands from model objects and tracks their dependencies.
// Stateless model→render translation (create-infos, queue ids, GPU packing,
// sampler mapping) lives in render_translate.h, not here.
class render_bridge
{
public:
    kryga::result_code
    render_cmd_build(root::smart_object& obj, bool sub_objects);

    kryga::result_code
    render_cmd_destroy(root::smart_object& obj, bool sub_objects);

    kryga::result_code
    render_cmd_transform(root::game_object_component& source);

    render_object_dependency_graph&
    get_dependency()
    {
        return m_dependency_graph;
    }

    // Raw allocation from the per-frame arena (implemented in .cpp)
    static void*
    alloc_cmd_raw(size_t size, size_t align);

    // Convenience: allocate a command from the per-frame arena
    template <typename T, typename... Args>
    T*
    alloc_cmd(Args&&... args);

    // Convenience: enqueue a command for render thread
    void
    enqueue_cmd(render_cmd::render_command_base* cmd);

private:
    render_object_dependency_graph m_dependency_graph;
};

template <typename T, typename... Args>
T*
render_bridge::alloc_cmd(Args&&... args)
{
    void* mem = alloc_cmd_raw(sizeof(T), alignof(T));
    return new (mem) T(std::forward<Args>(args)...);
}

}  // namespace kryga
