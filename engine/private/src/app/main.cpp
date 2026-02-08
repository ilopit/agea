#include <engine/kryga_engine.h>

#include <global_state/global_state.h>
#include <utils/kryga_log.h>

#if WIN32

#include <shellscalingapi.h>
#pragma comment(lib, "Shcore.lib")

#endif

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
        kryga::vulkan_engine engine;
        kryga::state_mutator__engine::set(&engine, kryga::glob::glob_state());

        engine.init(options);
        engine.run();
        engine.cleanup();
    }

    return 0;
}
