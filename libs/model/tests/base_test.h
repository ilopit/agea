#pragma once

#include <model/model_fwds.h>

#include <utils/singleton_instance.h>
#include <utils/path.h>

#include <filesystem>
#include <gtest/gtest.h>

namespace agea
{
struct base_test : public testing::Test
{
    void
    SetUp();

    void
    TearDown();

    agea::utils::path
    get_current_workspace();

    agea::singleton_registry m_regestry;
};
};  // namespace agea