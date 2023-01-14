#pragma once

#include <arl/arl_defines.h>
#include <glm_unofficial/glm.h>

namespace agea
{
namespace model
{

AGEA_struct();
struct vec2 : ::glm::vec2
{
    vec2()
        : glm::vec2()
    {
    }

    vec2(const glm::vec2& v)
        : glm::vec2(v)
    {
    }

    AGEA_ctor("category=world");
    vec2(float x, float y)
        : glm::vec2(x, y)
    {
    }

    const glm::vec2&
    as_glm() const
    {
        return *this;
    }

    glm::vec2&
    as_glm()
    {
        return *this;
    }

    // x
    AGEA_function("category=world");
    void
    set_x(float v)
    {
        x = v;
    }

    AGEA_function("category=world");
    float
    get_x() const
    {
        return x;
    }

    // y
    AGEA_function("category=world");
    void
    set_y(float v)
    {
        y = v;
    }

    AGEA_function("category=world");
    float
    get_y() const
    {
        return y;
    }
};

}  // namespace model
}  // namespace agea