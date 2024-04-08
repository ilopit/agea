#pragma once

#include <ar/ar_defines.h>
#include <glm_unofficial/glm.h>

namespace agea
{
namespace root
{
//     'auto rt = reflection_regestry.get_type(id);
//         rt->deserialization = default_deserialize<type_name>;
//     rt->compare = default_compare<type_name>;
//     rt->copy = default_copy<type_name>;
//     rt->serialization = default_serialize<type_name>;
//     rt->ui = simple_to_string<type_name>;
//     '
AGEA_ar_struct();
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