#include <gtest/gtest.h>

#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/kryga_render.h>

#include <global_state/global_state.h>
#include <vfs/vfs.h>
#include <vfs/vfs_state.h>
#include <vfs/physical_backend.h>

#include <utils/kryga_log.h>

int
main(int argc, char** argv)
{
    ::kryga::utils::setup_logger();

    auto& gs = kryga::glob::glob_state();

    kryga::state_mutator__vfs::set(gs);
    auto root = std::filesystem::current_path().parent_path();
    auto& vfs = gs.getr_vfs();
    vfs.mount("data", std::make_unique<kryga::vfs::physical_backend>(root), 0);
    vfs.mount("cache", std::make_unique<kryga::vfs::physical_backend>(root / "cache"), 0);
    vfs.mount("tmp", std::make_unique<kryga::vfs::physical_backend>(root / "tmp"), 0);
    vfs.mount(
        "generated",
        std::make_unique<kryga::vfs::physical_backend>(root.parent_path() / "kryga_generated"),
        0);

    kryga::state_mutator__render_device::set(gs);
    kryga::state_mutator__vulkan_render::set(gs);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}