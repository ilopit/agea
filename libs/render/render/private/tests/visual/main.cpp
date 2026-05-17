#include <gtest/gtest.h>

#include <vulkan_render/vulkan_render_device.h>
#include <vulkan_render/kryga_render.h>

#include <global_state/global_state.h>
#include <core/core_state.h>
#include <vfs/vfs.h>
#include <vfs/vfs_state.h>
#include <vfs/physical_backend.h>
#include <project_paths/project_paths.h>

#include <utils/kryga_log.h>

int
main(int argc, char** argv)
{
    ::kryga::utils::setup_logger();

    kryga::paths::set_exe_path(argv[0]);

    auto& gs = kryga::glob::glob_state();

    kryga::state_mutator__vfs::set(gs);
    auto root = std::filesystem::current_path().parent_path();
    auto& vfs = gs.getr_vfs();

    // Cooked tree at default priority — precooked .spv, .aobj, packaged
    // levels resolve here first. In dev, override `data://shaders_includes`
    // to point at the source tree so runtime glslc compile sees the .glsl
    // source files (the cooked tree only has .spv). gpu_types and resources/
    // are added as low-priority fallbacks for assets only in the source tree.
    vfs.mount("data", std::make_unique<kryga::vfs::physical_backend>(root), 10);

    if (auto layout = kryga::paths::resolve(); layout && layout->is_dev_layout)
    {
        // Source tree wins over cooked tree for source-only assets: glslc
        // needs .glsl source files for #includes at runtime, and the cooked
        // tree only has .spv. Priorities chosen so directory lookups for
        // data://shaders_includes and data://gpu_types resolve to the
        // source-tree paths the runtime compiler expects.
        vfs.mount(kryga::vfs::rid("data", "shaders_includes"),
                  layout->source_root / "resources" / "shaders_includes",
                  {.priority = 30});
        vfs.mount("data",
                  std::make_unique<kryga::vfs::physical_backend>(
                      layout->source_root / "libs" / "render" / "gpu_types" / "public" / "include"),
                  20);
        vfs.mount("data",
                  std::make_unique<kryga::vfs::physical_backend>(layout->source_root / "resources"),
                  0);
    }

    vfs.mount("cache", std::make_unique<kryga::vfs::physical_backend>(root / "cache"), 0);
    vfs.mount("tmp", std::make_unique<kryga::vfs::physical_backend>(root / "tmp"), 0);
    vfs.mount(
        "generated",
        std::make_unique<kryga::vfs::physical_backend>(root.parent_path() / "kryga_generated"),
        0);
    kryga::core::state_mutator__model::set(gs);
    kryga::state_mutator__render::set(gs);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
