#pragma once

#include <shader_system/shader_reflection.h>

#include <utils/buffer.h>
#include <error_handling/error_handling.h>
#include <expected>

namespace kryga::render
{

// TODO: Consider making reflection optional if shader count grows significantly
//       and some shaders don't need reflection data (e.g. simple post-process effects)
struct compiled_shader
{
    kryga::utils::buffer spirv;
    reflection::shader_reflection reflection;
};

using compilation_result = std::expected<compiled_shader, result_code>;

class shader_compiler
{
public:
    static compilation_result
    compile_shader(const kryga::utils::buffer& raw_buffer,
                   const std::vector<std::string>& defines = {});

private:
};

}  // namespace kryga::render
