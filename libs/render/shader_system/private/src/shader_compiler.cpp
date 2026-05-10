#include "shader_system/shader_compiler.h"

#include <utils/kryga_log.h>

#if !defined(__ANDROID__)
#include <utils/process.h>
#include <global_state/global_state.h>
#include <vfs/vfs.h>
#include <regex>
#endif

namespace kryga::render
{

#if defined(__ANDROID__)

compilation_result
shader_compiler::compile_shader(const kryga::utils::buffer& raw_buffer,
                                const std::vector<std::string>& /*defines*/)
{
    ALOG_ERROR("shader_compiler::compile_shader called on Android (vpath '{}'). "
               "Runtime GLSL compilation is unsupported — shader must be "
               "cooked via tools/cook with is_*_binary=true.",
               raw_buffer.get_vpath());
    KRG_never("Runtime shader compilation attempted on Android");
    return std::unexpected(compilation_error{result_code::compilation_failed});
}

#else

namespace
{

std::vector<shader_diagnostic>
parse_glslc_output(const std::string& output)
{
    std::vector<shader_diagnostic> diags;
    static const std::regex pattern(
        R"(([^:]+):(\d+):(?:(\d+):)?\s*(error|warning):\s*(.+))");
    std::sregex_iterator it(output.begin(), output.end(), pattern);
    std::sregex_iterator end;
    for (; it != end; ++it)
    {
        auto& m = *it;
        shader_diagnostic d;
        d.file = m[1].str();
        d.line = std::stoi(m[2].str());
        d.column = m[3].matched ? std::stoi(m[3].str()) : 0;
        d.severity = m[4].str();
        d.message = m[5].str();
        diags.push_back(std::move(d));
    }
    return diags;
}

}  // namespace

compilation_result
shader_compiler::compile_shader(const kryga::utils::buffer& raw_buffer,
                                const std::vector<std::string>& defines)
{
    static int shader_id = 0;

    auto& vfs = glob::glob_state().getr_vfs();

    ipc::construct_params params;
    auto tools_path = vfs.real_path(vfs::rid("data://tools"));
    params.path_to_binary = APATH(tools_path.value() / "glslc.exe");

    auto tmp_dir = vfs.create_temp_dir();
    params.working_dir = tmp_dir.folder();

    auto tmp_shader_name = APATH(std::to_string(shader_id++));

    kryga::utils::path compiled_path = tmp_dir.folder() / tmp_shader_name.str();
    compiled_path.add(".spv");

    if (compiled_path.exists())
    {
        ALOG_ERROR("Tmp file already exists!");
        return std::unexpected(compilation_error{result_code::failed});
    }

    auto includes = vfs.real_path(vfs::rid("data://shaders_includes"));
    auto gpu_includes = vfs.real_path(vfs::rid("data://gpu_types"));
    auto generated_gpu_includes = vfs.real_path(vfs::rid("generated"));

    auto gpu_includes_root = gpu_includes.value().parent_path();

    params.arguments =
        std::format("-V {0} -o {1} --target-env=vulkan1.2 --target-spv=spv1.5 -I {2} -I {3} -I {4}",
                    raw_buffer.get_file().str(),
                    compiled_path.str(),
                    APATH(includes.value()).str(),
                    APATH(gpu_includes_root).str(),
                    APATH(generated_gpu_includes.value()).str());

    for (const auto& def : defines)
    {
        params.arguments += " -D" + def;
    }

    uint64_t rc = 0;
    if (!ipc::run_binary(params, rc) || rc != 0)
    {
        // Try again with capture to get structured error info for diagnostics.
        std::string captured;
        ipc::run_binary_capture(params, rc, captured);

        ALOG_ERROR("Shader compilation failed: {}", captured);
        compilation_error err;
        err.code = result_code::compilation_failed;
        err.raw_output = std::move(captured);
        err.diagnostics = parse_glslc_output(err.raw_output);
        return std::unexpected(std::move(err));
    }

    compiled_shader cs;
    if (!kryga::utils::buffer::load(compiled_path, cs.spirv))
    {
        ALOG_ERROR("Failed to load compiled shader");
        return std::unexpected(compilation_error{result_code::compilation_failed});
    }

    if (!shader_reflection_utils::build_shader_reflection(
            cs.spirv.data(), cs.spirv.size(), cs.reflection))
    {
        ALOG_ERROR("Failed to build shader reflection");
        return std::unexpected(compilation_error{result_code::compilation_failed});
    }

    return cs;
}

#endif

}  // namespace kryga::render
