#pragma once

#include "core/screen.h"

#include <utils/id.h>

#include <memory>
#include <vector>

namespace kryga::core
{

// Owns the stack of live UI screens. Held by model_system (screens), peer to
// level_manager / package_manager. A stack so the standard runtime-UI pattern
// works directly: push a pause screen over the HUD, pop to resume. The top of
// the stack is the active screen.
class screen_manager
{
public:
    // Push a new empty screen and make it active. Returns it (never null).
    screen*
    push(const utils::id& id);

    // Pop and render-safe-unload the top screen. Asserts the stack is non-empty.
    void
    pop();

    // Top of stack, or null if there are no screens.
    screen*
    active();

    // Find a screen by id anywhere in the stack, or null.
    screen*
    find(const utils::id& id);

    // Per-frame update: ticks every screen in the stack (so a HUD under a pause
    // screen still animates). Pause semantics, if needed, belong on the screen.
    void
    tick(float dt);

    // Unload + drop every screen.
    void
    clear();

private:
    std::vector<std::unique_ptr<screen>> m_stack;
};

}  // namespace kryga::core
