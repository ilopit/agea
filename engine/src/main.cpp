#include <engine/agea_engine.h>

#include <utils/agea_log.h>
#include <utils/singleton_registry.h>

#include <memory>

#if WIN32

#include <shellscalingapi.h>
#pragma comment(lib, "Shcore.lib")

#endif

int
main(int, char*)
{
    agea::utils::setup_logger();
    auto registry = std::make_unique<agea::singleton_registry>();
#if WIN32
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
#endif
    {
        auto& r = *registry;
        agea::glob::engine::create(r, std::move(registry));

        agea::glob::engine::getr().init();
        agea::glob::engine::getr().run();
        agea::glob::engine::getr().cleanup();
    }

    return 0;
}
