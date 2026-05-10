#pragma once

#include <shader_system/shader_reflection.h>

#include <utils/buffer.h>
#include <error_handling/error_handling.h>
#include <expected>
#include <string>
#include <vector>

namespace kryga::render
{

struct compiled_shader
{
    kryga::utils::buffer spirv;
    reflection::shader_reflection reflection;
};

struct shader_diagnostic
{
    std::string file;
    int line = 0;
    int column = 0;
    std::string severity;
    std::string message;
};

struct compilation_error
{
    result_code code = result_code::compilation_failed;
    std::string raw_output;
    std::vector<shader_diagnostic> diagnostics;
};

using compilation_result = std::expected<compiled_shader, compilation_error>;

class shader_compiler
{
public:
    static compilation_result
    compile_shader(const kryga::utils::buffer& raw_buffer,
                   const std::vector<std::string>& defines = {});

private:
};

}  // namespace kryga::render
