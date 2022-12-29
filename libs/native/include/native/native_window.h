#pragma once

#include <utils/singleton_instance.h>

struct SDL_Window;

namespace agea
{
class native_window
{
public:
    struct size
    {
        int w;
        int h;
    };

    struct construct_params
    {
        int w = 1600;
        int h = 900;
    };

    bool
    construct(construct_params& c);

    struct SDL_Window*
    handle() const
    {
        return m_window;
    }

    float
    aspect_ratio()
    {
        return float(get_size().w) / get_size().h;
    }

    size
    get_size();

private:
    struct SDL_Window* m_window = nullptr;
};

namespace glob
{
struct native_window : public ::agea::singleton_instance<::agea::native_window, native_window>
{
};
}  // namespace glob

}  // namespace agea
