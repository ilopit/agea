#include "shader_system/shader_compiler.h"

#include <utils/process.h>
#include <utils/kryga_log.h>
#include <global_state/global_state.h>
#include <vfs/vfs.h>

namespace kryga::render
{

compilation_result
shader_compiler::compile_shader(const kryga::utils::buffer& raw_buffer)
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

    // Build reflection data from SPIR-V
    if (!shader_reflection_utils::build_shader_reflection(
            cs.spirv.data(), cs.spirv.size(), cs.reflection))
    {
        ALOG_ERROR("Failed to build shader reflection");
        return std::unexpected(result_code::compilation_failed);
    }

    return cs;
}

}  // namespace kryga::render
