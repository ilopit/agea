#pragma once

#include <gtest/gtest.h>

#include <shader_system/shader_compiler.h>
#include <shader_system/shader_reflection.h>
#include <global_state/global_state.h>
#include <vfs/vfs.h>
#include <utils/file_utils.h>

namespace kryga::render::test
{

inline void
print_layout(const ::kryga::utils::dynobj_layout_sptr l)
{
    auto view = l->make_view<gpu_type>();

    std::string tmp;
    view.print(tmp);
    std::cout << tmp << std::endl;
}

inline void
print_reflection(const reflection::shader_reflection& rlf)
{
    if (rlf.constants)
    {
        print_layout(rlf.constants->layout);
    }
    for (auto& dsc : rlf.descriptors)
    {
        for (auto& bnd : dsc.bindings)
        {
            print_layout(bnd.layout);
        }
    }

    print_layout(rlf.input_interface.layout);

    print_layout(rlf.output_interface.layout);
}

class shader_compiler_test : public ::testing::Test
{
protected:
    void
    SetUp() override
    {
        auto rt_rp = glob::glob_state().getr_vfs().real_path(vfs::rid("tmp"));
        auto rt = APATH(rt_rp.value());

        std::filesystem::remove_all(rt.fs());

        m_temp_dir = glob::glob_state().getr_vfs().create_temp_dir();
    }

    void
    TearDown() override
    {
        // temp_dir_context destructor cleans up
    }

    utils::buffer
    create_shader_buffer(const std::string& source, const std::string& filename)
    {
        utils::path shader_path = m_temp_dir.folder() / filename;

        std::vector<uint8_t> data(source.begin(), source.end());
        utils::file_utils::save_file(shader_path, data);

        utils::buffer buf;
        utils::buffer::load(shader_path, buf);
        return buf;
    }

    temp_dir_context m_temp_dir;
};

}  // namespace kryga::render::test
