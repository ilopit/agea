#include <engine/kryga_engine.h>

#include <global_state/global_state.h>
#include <utils/kryga_log.h>
#include <vfs/vfs.h>
#include <vfs/vfs_state.h>
#include <vfs/physical_backend.h>

#if WIN32

#include <windows.h>
#include <shellscalingapi.h>
#pragma comment(lib, "Shcore.lib")

#endif

static std::filesystem::path
get_exe_dir()
{
#if WIN32
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
#else
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

int
main(int argc, char** argv)
{
    kryga::utils::setup_logger(spdlog::level::level_enum::trace);

    // Parse command-line arguments
    kryga::startup_options options;
    if (!kryga::startup_options::parse(argc, argv, options))
    {
        if (options.show_help)
        {
            kryga::startup_options::print_help(argv[0]);
            return 0;
        }
        return 1;
    }

#if WIN32
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
#endif
    {
        auto& gs = kryga::glob::glob_state();

        kryga::state_mutator__vfs::set(gs);
        auto root = get_exe_dir().parent_path();
        auto& vfs = gs.getr_vfs();
        vfs.mount("data", std::make_unique<kryga::vfs::physical_backend>(root), 0);
        vfs.mount("cache", std::make_unique<kryga::vfs::physical_backend>(root / "cache"), 0);
        vfs.mount("rtcache", std::make_unique<kryga::vfs::physical_backend>(root / "rtcache"), 0);
        vfs.mount("tmp", std::make_unique<kryga::vfs::physical_backend>(root / "tmp"), 0);
        vfs.mount(
            "generated",
            std::make_unique<kryga::vfs::physical_backend>(root.parent_path() / "kryga_generated"),
            0);

        kryga::vulkan_engine engine;
        kryga::state_mutator__engine::set(&engine, gs);

        engine.init(options);
        engine.run();
        engine.cleanup();
    }

    return 0;
}
