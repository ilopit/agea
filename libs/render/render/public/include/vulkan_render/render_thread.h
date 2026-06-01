#pragma once

#include <utils/check.h>

namespace kryga::render
{

// Per-thread render-access permission — thread_local, no shared state, no atomic.
// Default FALSE: no thread may mutate render state until it explicitly grants
// itself access. Exactly one thread holds it at a time:
//   - render thread: grants itself once in frame_pipeline::render_loop;
//   - engine main:   grants at vulkan_engine::init for single-threaded setup, hands
//                    off (false) before the streaming loop, reclaims after it;
//   - tests:         grant explicitly in their fixture (single-threaded).
// So a render-state mutation from a thread that didn't grant access (e.g. main
// mid-stream) trips the assert.
inline thread_local bool t_render_access = false;

inline void
set_render_access(bool allowed)
{
    t_render_access = allowed;
}

inline bool
on_render_thread()
{
    return t_render_access;
}

}  // namespace kryga::render

#define KRG_check_render_thread()                  \
    KRG_check(::kryga::render::on_render_thread(), \
              "render-state op ran off the render-owning thread")
