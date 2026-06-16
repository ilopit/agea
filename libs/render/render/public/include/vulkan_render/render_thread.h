#pragma once

#include <utils/check.h>

namespace kryga::render
{

// Per-thread render-access permission — thread_local, no shared state, no atomic.
// Default FALSE: no thread may mutate render state until it explicitly grants
// itself access. Exactly one thread holds it at a time:
//   - render thread: grants itself once in engine_threads::render_loop;
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

// Per-thread MODEL-access permission — symmetric to render access, for the
// model-owned content allocator (reserve/free/tick). The build thread (engine
// main) grants this once at init and holds it for the process lifetime — it is
// always the model thread and never hands off (unlike render access, which
// migrates to the render thread for streaming). The render thread never grants
// it. Single-threaded setups (init, tests) grant BOTH, so a thread doing both
// model and render work passes either check.
inline thread_local bool t_model_access = false;

inline void
set_model_access(bool allowed)
{
    t_model_access = allowed;
}

inline bool
on_model_thread()
{
    return t_model_access;
}

}  // namespace kryga::render

#define KRG_check_render_thread()                  \
    KRG_check(::kryga::render::on_render_thread(), \
              "render-state op ran off the render-owning thread")

#define KRG_check_model_thread()                  \
    KRG_check(::kryga::render::on_model_thread(), \
              "model-state op ran off the model-owning thread")

// Debug-only variants for hot paths (e.g. per-draw handle resolution): active in
// Debug, compiled out under NDEBUG.
#ifndef NDEBUG
#define KRG_check_render_thread_dbg() KRG_check_render_thread()
#define KRG_check_model_thread_dbg() KRG_check_model_thread()
#else
#define KRG_check_render_thread_dbg() ((void)0)
#define KRG_check_model_thread_dbg() ((void)0)
#endif
