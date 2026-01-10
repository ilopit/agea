#pragma once

#include <utils/buffer.h>
#include <error_handling/error_handling.h>
#include <expected>

namespace kryga::render
{
struct compiled_shader
{
    kryga::utils::buffer raw_data;
};

using compilation_result = std::expected<compiled_shader, result_code>;

class shader_compiler
{
public:
    static compilation_result
    compile_shader(const kryga::utils::buffer& raw_buffer);

private:
};

}  // namespace kryga::render
