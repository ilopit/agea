#include "vk_engine.h"

int
main(int argc, char* argv[])
{
    auto engine_closure = agea::glob::engine::create();

    auto engine = agea::glob::engine::get();

    engine->init();
    engine->run();
    engine->cleanup();

    return 0;
}
