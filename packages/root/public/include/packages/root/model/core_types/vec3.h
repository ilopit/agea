#pragma once

#include <ar/ar_defines.h>
#include <glm_unofficial/glm.h>

namespace agea
{
namespace root
{
// clang-format off
AGEA_ar_struct(copy_handler        = ::agea::reflection::utils::cpp_copy<::agea::root::vec2>,
               compare_handler     = ::agea::reflection::utils::cpp_compare<::agea::root::vec2>,
               serialize_handler   = ::agea::root::vec3__serialize,
               deserialize_handler = ::agea::root::vec3__deserialize,
               to_string_handler   = ::agea::root::vec3__to_string);
struct vec3 : ::glm::vec3
// clang-format on
{
    vec3()
        : glm::vec3()
    {
    }

    vec3(const glm::vec3& v)
        : glm::vec3(v)
    {
    }

    AGEA_ar_ctor("category=world");
    vec3(float x, float y, float z)
        : glm::vec3(x, y, z)
    {
    }

    AGEA_ar_ctor("category=world");
    vec3(float v)
        : glm::vec3(v)
    {
    }

    const glm::vec3&
    as_glm() const
    {
        return *this;
    }

    glm::vec3&
    as_glm()
    {
        return *this;
    }

    // x
    AGEA_ar_function("category=world");
    void
    set_x(float v)
    {
        x = v;
    }

    AGEA_ar_function("category=world");
    float
    get_x() const
    {
        return x;
    }

    // y
    AGEA_ar_function("category=world");
    void
    set_y(float v)
    {
        y = v;
    }

    AGEA_ar_function("category=world");
    float
    get_y() const
    {
        return y;
    }

    // z
    AGEA_ar_function("category=world");
    void
    set_z(float v)
    {
        z = v;
    }

    AGEA_ar_function("category=world");
    float
    get_z() const
    {
        return z;
    }
};

}  // namespace root
}  // namespace agea