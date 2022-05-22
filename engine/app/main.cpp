#include <core/vk_engine.h>
#include <utils/agea_log.h>

#if WIN32

#include <shellscalingapi.h>
#pragma comment(lib, "Shcore.lib")

#endif

int
main(int, char*)
{
    agea::utils::setup_logger();
#if WIN32
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
#endif
    {
        auto engine_closure = agea::glob::engine::create();

        auto engine = agea::glob::engine::get();

        engine->init();
        engine->run();
        engine->cleanup();
    }

    return 0;
}
