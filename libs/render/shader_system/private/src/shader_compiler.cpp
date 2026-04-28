#include "shader_system/shader_compiler.h"

#include <utils/kryga_log.h>

#if !defined(__ANDROID__)
#include <utils/process.h>
#include <global_state/global_state.h>
#include <vfs/vfs.h>
#endif

namespace kryga::render
{

#if defined(__ANDROID__)

// Android ships pre-cooked SPIR-V (tools/cook runs at host build time and the
// gradle pipeline copies the cooked tree into APK assets). Runtime GLSL
// compilation would require glslc + writable scratch + the include tree to be
// reachable via real_path, none of which hold inside an APK. Reaching this
// path means a non-binary shader slipped through cooking — fail loudly so the
// asset gets fixed instead of silently producing a black screen.
compilation_result
shader_compiler::compile_shader(const kryga::utils::buffer& raw_buffer,
                                const std::vector<std::string>& /*defines*/)
{
    ALOG_ERROR("shader_compiler::compile_shader called on Android (vpath '{}'). "
               "Runtime GLSL compilation is unsupported — shader must be "
               "cooked via tools/cook with is_*_binary=true.",
               raw_buffer.get_vpath());
    KRG_never("Runtime shader compilation attempted on Android");
    return std::unexpected(result_code::compilation_failed);
}

#else

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
        return std::unexpected(result_code::failed);
    }

    auto includes = vfs.real_path(vfs::rid("data://shaders_includes"));
    auto gpu_includes = vfs.real_path(vfs::rid("data://gpu_types"));
    auto generated_gpu_includes = vfs.real_path(vfs::rid("generated"));

    params.arguments =
        std::format("-V {0} -o {1} --target-env=vulkan1.2 --target-spv=spv1.5 -I {2} -I {3} -I {4}",
                    raw_buffer.get_file().str(),
                    compiled_path.str(),
                    APATH(includes.value()).str(),
                    APATH(gpu_includes.value()).str(),
                    APATH(generated_gpu_includes.value()).str());

    for (const auto& def : defines)
    {
        params.arguments += " -D" + def;
    }

    uint64_t rc = 0;
    if (!ipc::run_binary(params, rc) || rc != 0)
    {
        ALOG_ERROR("Shader compilation failed");
        return std::unexpected(result_code::compilation_failed);
    }

    compiled_shader cs;
    if (!kryga::utils::buffer::load(compiled_path, cs.spirv))
    {
        ALOG_ERROR("Failed to load compiled shader");
        return std::unexpected(result_code::compilation_failed);
    }

    if (!shader_reflection_utils::build_shader_reflection(
            cs.spirv.data(), cs.spirv.size(), cs.reflection))
    {
        ALOG_ERROR("Failed to build shader reflection");
        return std::unexpected(result_code::compilation_failed);
    }

    return cs;
}

#endif

}  // namespace kryga::render
