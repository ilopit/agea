#pragma once

#include <ar/ar_defines.h>
#include <glm_unofficial/glm.h>

namespace kryga
{
namespace root
{

// clang-format off
KRG_ar_struct(
    copy_handler      = ::kryga::reflection::utils::cpp_default__copy<::kryga::root::vec4>,
    compare_handler   = ::kryga::reflection::utils::cpp_default__compare<::kryga::root::vec4>,
    json_save_handler = ::kryga::root::vec4__json_save,
    json_load_handler = ::kryga::root::vec4__json_load,
    mcp_schema        = "array:number:4",
    mcp_hint          = "4D vector [x y z w]"
);
struct vec4 : ::glm::vec4
// clang-format on
{
    vec4()
        : glm::vec4()
    {
    }

    vec4(const glm::vec4& v)
        : glm::vec4(v)
    {
    }

    KRG_ar_ctor("category=world");
    vec4(float x, float y, float z, float w)
        : glm::vec4(x, y, z, w)
    {
    }

    KRG_ar_ctor("category=world");
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
    // clang-format off
    KRG_ar_function(
        category = "world"
    );
    void
    set_x(float v)
    // clang-format on
    {
        x = v;
    }

    // clang-format off
    KRG_ar_function(
        category = "world"
    );
    float
    get_x() const
    // clang-format on
    {
        return x;
    }

    // y
    // clang-format off
    KRG_ar_function(
        category = "world"
    );
    void
    set_y(float v)
    // clang-format on
    {
        y = v;
    }

    // clang-format off
    KRG_ar_function(
        category = "world"
    );
    float
    get_y() const
    // clang-format on
    {
        return y;
    }

    // z
    // clang-format off
    KRG_ar_function(
        category = "world"
    );
    void
    set_z(float v)
    // clang-format on
    {
        z = v;
    }

    // clang-format off
    KRG_ar_function(
        category = "world"
    );
    float
    get_z() const
    // clang-format on
    {
        return z;
    }

    // w
    // clang-format off
    KRG_ar_function(
        category = "world"
    );
    void
    set_w(float v)
    // clang-format on
    {
        w = v;
    }

    // clang-format off
    KRG_ar_function(
        category = "world"
    );
    float
    get_w() const
    // clang-format on
    {
        return w;
    }
};

}  // namespace root
}  // namespace kryga