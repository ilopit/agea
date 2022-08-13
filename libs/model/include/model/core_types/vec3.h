#pragma once

#include <arl/arl_defines.h>
#include <glm_unofficial/glm.h>

namespace agea
{
namespace model
{

AGEA_struct();
struct vec3 : ::glm::vec3
{
    vec3()
        : glm::vec3()
    {
    }

    vec3(const glm::vec3& v)
        : glm::vec3(v)
    {
    }

    AGEA_ctor("category=world");
    vec3(float x, float y, float z)
        : glm::vec3(x, y, z)
    {
    }

    AGEA_ctor("category=world");
    vec3(float v)
        : glm::vec3(v)
    {
    }

    glm::vec3
    as_glm() const
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

    std::string
    to_string()
    {
        return "bla";
    }
};

}  // namespace model
}  // namespace agea