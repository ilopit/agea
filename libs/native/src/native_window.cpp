#include "native/native_window.h"

#include <SDL.h>
#include <SDL_vulkan.h>

namespace agea
{

glob::native_window::type glob::native_window::type::s_instance;

bool
native_window::construct(construct_params& c)
{
    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    m_window = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                c.w, c.h, window_flags);

    return !!m_window;
}

native_window::size
native_window::get_size()
{
    native_window::size s;
    SDL_GetWindowSize(m_window, &s.w, &s.h);

    return s;
}

}  // namespace agea
