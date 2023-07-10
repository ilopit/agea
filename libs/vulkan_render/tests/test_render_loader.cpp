
#include <gtest/gtest.h>

#include <vulkan_render/vulkan_render_loader.h>
#include <vulkan_render/vulkan_render_device.h>

#include <vulkan_render/types/vulkan_shader_effect_data.h>
#include <resource_locator/resource_locator.h>

using namespace agea;
using namespace agea::render;

class render_device_test : public ::testing::Test
{
public:
    void
    SetUp()
    {
        render::render_device::construct_params rdc;
        rdc.headless = true;

        auto device = glob::render_device::get();
        ASSERT_TRUE(device->construct(rdc));

        m_registry = std::make_unique<agea::singleton_registry>();
        glob::vulkan_render_loader::create(*m_registry);
    }

    void
    TearDown()
    {
        glob::vulkan_render_loader::getr().clear_caches();
        glob::render_device::get()->destruct();
    }

    std::unique_ptr<agea::singleton_registry> m_registry;
};

TEST_F(render_device_test, load)
{
    agea::utils::buffer vert, frag;

    auto path = glob::resource_locator::get()->resource(category::packages,
                                                        "base.apkg/class/shader_effects/error");

    auto vert_path = path / "se_error.vert";
    agea::utils::buffer::load(vert_path, vert);

    auto frag_path = path / "se_error.frag";
    agea::utils::buffer::load(frag_path, frag);

    shader_effect_create_info se_ci;
    se_ci.vert_buffer = &vert;
    se_ci.frag_buffer = &frag;
    se_ci.render_pass = glob::render_device::getr().render_pass();
    se_ci.is_wire = false;
    se_ci.enable_dynamic_state = false;
    se_ci.enable_alpha = false;
    se_ci.cull_mode = VK_CULL_MODE_BACK_BIT;
    se_ci.width = 1024;
    se_ci.height = 1024;

    shader_effect_data* sed = nullptr;
    auto rc = glob::vulkan_render_loader::getr().create_shader_effect(AID("se_error"), se_ci, sed);

    ASSERT_EQ(rc, result_code::ok);
    ASSERT_TRUE(sed);
}