
#include <gtest/gtest.h>

#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/kryga_render.h>

#include <vulkan_render/types/vulkan_shader_effect_data.h>
#include <vulkan_render/types/vulkan_render_pass.h>
#include <global_state/global_state.h>
#include <resource_locator/resource_locator.h>

using namespace kryga;
using namespace kryga::render;

class render_device_test : public ::testing::Test
{
public:
    void
    SetUp()
    {
        render::render_device::construct_params rdc;
        rdc.headless = true;

        auto device = glob::glob_state().get_render_device();
        ASSERT_TRUE(device->construct(rdc));

        state_mutator__vulkan_render_loader::set(glob::glob_state());

        glob::glob_state().getr_vulkan_render().init(500, 500, render_mode::instanced, true);
    }

    void
    TearDown()
    {
        glob::glob_state().get_vulkan_render()->deinit();
        glob::glob_state().getr_vulkan_render_loader().clear_caches();
        glob::glob_state().get_render_device()->destruct();
    }
};

TEST_F(render_device_test, load_se)
{
    kryga::utils::buffer vert, frag;

    auto path = glob::glob_state().get_resource_locator()->resource(
        category::packages, "base.apkg/class/shader_effects/error");

    auto vert_path = path / "se_error.vert";
    kryga::utils::buffer::load(vert_path, vert);

    auto frag_path = path / "se_error.frag";
    kryga::utils::buffer::load(frag_path, frag);

    auto main_pass = glob::glob_state().getr_vulkan_render_loader().get_render_pass(AID("main"));

    shader_effect_create_info se_ci;
    se_ci.vert_buffer = &vert;
    se_ci.frag_buffer = &frag;
    se_ci.enable_dynamic_state = false;
    se_ci.alpha = alpha_mode::none;
    se_ci.cull_mode = VK_CULL_MODE_BACK_BIT;
    se_ci.width = 1024;
    se_ci.height = 1024;

    shader_effect_data* sed = nullptr;
    auto rc = main_pass->create_shader_effect(AID("se_error"), se_ci, sed);

    ASSERT_EQ(rc, result_code::ok);
    ASSERT_TRUE(sed);
}