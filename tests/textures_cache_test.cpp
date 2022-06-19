#include <model/caches/textures_cache.h>

#include <gtest/gtest.h>

#include <core/fs_locator.h>
#include <vulkan_render/render_loader.h>
#include <vulkan_render/render_device.h>

using namespace agea;

struct textures_viewer_backend_test : public testing::Test
{
    void
    SetUp()
    {
        m_resource_locator = glob::resource_locator::create();
        m_render_device = glob::render_device::create();
        m_render_loader = glob::render_loader::create();

        render::render_device::construct_params cp;
        cp.headless = true;
        cp.window = nullptr;
        glob::render_device::get()->construct(cp);
    }

    void
    TearDown()
    {
        m_render_loader.reset();
        m_resource_locator.reset();
        m_render_device.reset();
    }

    std::unique_ptr<closure<render::render_device>> m_render_device;
    std::unique_ptr<closure<resource_locator>> m_resource_locator;
    std::unique_ptr<closure<render::loader>> m_render_loader;
};

TEST_F(textures_viewer_backend_test, happy_run)
{
}