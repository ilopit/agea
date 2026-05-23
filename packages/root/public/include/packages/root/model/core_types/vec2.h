#pragma once

#include <ar/ar_defines.h>
#include <glm_unofficial/glm.h>

namespace kryga
{
namespace root
{

// clang-format off
KRG_ar_struct(
    copy_handler      = ::kryga::reflection::utils::cpp_default__copy<::kryga::root::vec2>,
    compare_handler   = ::kryga::reflection::utils::cpp_default__compare<::kryga::root::vec2>,
    json_save_handler = ::kryga::root::vec2__json_save,
    json_load_handler = ::kryga::root::vec2__json_load,
    mcp_schema        = "array:number:2",
    mcp_hint          = "2D vector [x y]"
);
struct vec2 : ::glm::vec2
// clang-format on
{
    vec2()
        : glm::vec2()
    {
    }

    vec2(const glm::vec2& v)
        : glm::vec2(v)
    {
    }

    KRG_ar_ctor("category=world");
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
};

}  // namespace root
}  // namespace kryga