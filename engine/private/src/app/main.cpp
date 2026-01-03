#include <engine/kryga_engine.h>

#include <utils/kryga_log.h>
#include <utils/singleton_registry.h>
#include <utils/static_initializer.h>

#include <memory>

#if WIN32

#include <shellscalingapi.h>
#pragma comment(lib, "Shcore.lib")

#endif

int
main(int argc, char** argv)
{
    kryga::utils::setup_logger(spdlog::level::level_enum::trace);

    auto registry = std::make_unique<kryga::singleton_registry>();

#if WIN32
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
#endif
    {
        auto& r = *registry;
        kryga::glob::engine::create(r, std::move(registry));

        kryga::glob::engine::getr().init();
        kryga::glob::engine::getr().run();
        kryga::glob::engine::getr().cleanup();
    }

    return 0;
}
