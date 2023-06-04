#include "native/native_window.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <resource_locator/resource_locator.h>

#include <stb_unofficial/stb.h>

namespace agea
{

glob::native_window::type glob::native_window::type::s_instance;

bool
native_window::construct(construct_params& c)
{
    // We initialize SDL and create a window with it.

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    m_window = SDL_CreateWindow("AGEA v0.1", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, c.w,
                                c.h, window_flags);

    auto icon_path = glob::resource_locator::getr().resource(category::editor, "icon_256.png");

    int tex_width = 0, tex_height = 0, tex_channels = 0;
    void* pixels =
        stbi_load(icon_path.str().c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

    Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    int shift = (my_icon.bytes_per_pixel == 3) ? 8 : 0;
    rmask = 0xff000000 >> shift;
    gmask = 0x00ff0000 >> shift;
    bmask = 0x0000ff00 >> shift;
    amask = 0x000000ff >> shift;
#else  // little endian, like x86
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = (4 == 3) ? 0 : 0xff000000;
#endif

    SDL_Surface* icon = SDL_CreateRGBSurfaceFrom((void*)pixels, tex_width, tex_height, 4 * 8,
                                                 4 * tex_width, rmask, gmask, bmask, amask);

    SDL_SetWindowIcon(m_window, icon);

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
