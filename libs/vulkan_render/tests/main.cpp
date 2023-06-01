#include <gtest/gtest.h>

#include "utils/agea_log.h"

#include <utils/singleton_registry.h>

int
main(int argc, char** argv)
{
    ::agea::utils::setup_logger();


    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}