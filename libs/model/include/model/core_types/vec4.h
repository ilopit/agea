#pragma once

#include <arl/arl_defines.h>
#include <glm_unofficial/glm.h>

namespace agea
{
namespace model
{

AGEA_struct();
struct vec4 : ::glm::vec4
{
    vec4()
        : glm::vec4()
    {
    }

    vec4(const glm::vec4& v)
        : glm::vec4(v)
    {
    }

    AGEA_ctor("category=world");
    vec4(float x, float y, float z, float w)
        : glm::vec4(x, y, z, w)
    {
    }

    AGEA_ctor("category=world");
    vec4(float v)
        : glm::vec4(v)
    {
    }

    const glm::vec4&
    as_glm() const
    {
        return *this;
    }

    glm::vec4&
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

    // z
    AGEA_function("category=world");
    void
    set_z(float v)
    {
        z = v;
    }

    AGEA_function("category=world");
    float
    get_z() const
    {
        return z;
    }

    // w
    AGEA_function("category=world");
    void
    set_w(float v)
    {
        w = v;
    }

    AGEA_function("category=world");
    float
    get_w() const
    {
        return w;
    }
};

}  // namespace model
}  // namespace agea