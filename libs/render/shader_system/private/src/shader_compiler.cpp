#include "shader_system/shader_compiler.h"

#include <utils/process.h>
#include <global_state/global_state.h>
#include <resource_locator/resource_locator.h>

namespace kryga::render
{

compilation_result
shader_compiler::compile_shader(const kryga::utils::buffer& raw_buffer)
{
    static int shader_id = 0;

    ipc::construct_params params;
    params.path_to_binary =
        (glob::glob_state().get_resource_locator()->resource_dir(category::tools) / "glslc.exe");

    auto tmp_dir = glob::glob_state().get_resource_locator()->temp_dir();
    params.working_dir = tmp_dir.folder();

    auto tmp_shader_name = APATH(std::to_string(shader_id));

    kryga::utils::path compiled_path = tmp_dir.folder() / tmp_shader_name.str();
    compiled_path.add(".spv");

    if (compiled_path.exists())
    {
        ALOG_ERROR("Tmp file already exists!");
        return std::unexpected(result_code::failed);
    }

    auto includes =
        glob::glob_state().get_resource_locator()->resource_dir(category::shaders_includes);
    auto gpu_includes =
        glob::glob_state().get_resource_locator()->resource_dir(category::shaders_gpu_data);

    params.arguments = std::format(
        "-V {0} -o {1} --target-env=vulkan1.2 --target-spv=spv1.5 -I {2} -I {3}",
        raw_buffer.get_file().str(), compiled_path.str(), includes.str(), gpu_includes.str());

    uint64_t rc = 0;
    if (!ipc::run_binary(params, rc) || rc != 0)
    {
        ALOG_ERROR("Shader compilation failed");
        return std::unexpected(result_code::compilation_failed);
    }

    compiled_shader cs;
    if (!kryga::utils::buffer::load(compiled_path, cs.raw_data))
    {
        ALOG_ERROR("Failed to load shader");
        return std::unexpected(result_code::compilation_failed);
    }

    return cs;
}

}  // namespace kryga::render
