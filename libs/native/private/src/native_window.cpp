#include "native/native_window.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <global_state/global_state.h>

#include <vfs/vfs.h>
#include <utils/path.h>

#include <stb_unofficial/stb.h>

namespace kryga
{

void
state_mutator__native_window::set(gs::state& s)
{
    auto p = s.create_box<native_window>("native_window");
    s.m_native_window = p;
}

bool
native_window::construct(construct_params& c)
{
    // We initialize SDL and create a window with it.

#if defined(__ANDROID__)
    // Let SDL synthesize SDL_MOUSE* events from SDL_FINGER events. ImGui's
    // SDL2 backend reads mouse events, not finger events — without this hint
    // set to "1", taps never propagate to ImGui and UI is unclickable on
    // Android. `input_manager` filters synthesized mouse events by checking
    // `SDL_MOUSEMOTION.which == SDL_TOUCH_MOUSEID` to avoid duplicate input.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
    // Don't synthesize finger events from mouse — we don't have a real mouse
    // on Android so this would only matter on Chromebooks etc.
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
#endif

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER);

    Uint32 flags = SDL_WINDOW_VULKAN;
    if (c.hidden)
    {
        flags |= SDL_WINDOW_HIDDEN;
    }
#if defined(__ANDROID__)
    // On Android the window is fullscreen — dimensions are dictated by the
    // surface provided by the Activity, not the construct_params values.
    flags |= SDL_WINDOW_FULLSCREEN;
#else
    // Desktop fullscreen for present-latency experiments. Borderless desktop
    // (1) lets the Windows compositor promote to independent flip; exclusive
    // (2) takes the hardware flip path (lowest latency). Swapchain extent is
    // read from SDL after creation, so any desktop resolution is handled.
    if (c.fullscreen == 1)
    {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    else if (c.fullscreen == 2)
    {
        flags |= SDL_WINDOW_FULLSCREEN;
    }
#endif

    m_window = SDL_CreateWindow("KRYGA v0.1",
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                c.w,
                                c.h,
                                static_cast<SDL_WindowFlags>(flags));

    // Hidden headless windows don't need an icon — skip filesystem + surface setup.
    if (c.hidden)
    {
        return !!m_window;
    }

    auto icon_rp = glob::glob_state().getr_vfs().real_path(vfs::rid("data://editor/icon_256.png"));

    int tex_width = 0, tex_height = 0, tex_channels = 0;
    void* pixels = icon_rp.has_value() ? stbi_load(APATH(icon_rp.value()).str().c_str(),
                                                   &tex_width,
                                                   &tex_height,
                                                   &tex_channels,
                                                   STBI_rgb_alpha)
                                       : nullptr;

    Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    int shift = (tex_channels == 3) ? 8 : 0;
    rmask = 0xff000000 >> shift;
    gmask = 0x00ff0000 >> shift;
    bmask = 0x0000ff00 >> shift;
    amask = 0x000000ff >> shift;
#else  // little endian, like x86
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = (tex_channels == 3) ? 0 : 0xff000000;
#endif

    SDL_Surface* icon = SDL_CreateRGBSurfaceFrom(
        (void*)pixels, tex_width, tex_height, 4 * 8, 4 * tex_width, rmask, gmask, bmask, amask);

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

}  // namespace kryga
