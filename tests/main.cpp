#include <gtest/gtest.h>

#include "utils/agea_log.h"

#include "reflection/property.h"

int
main(int argc, char** argv)
{
    ::agea::utils::setup_logger();
    ::agea::reflection::entry::set_up();

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}