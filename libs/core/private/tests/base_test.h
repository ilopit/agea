#pragma once

#include <core/model_fwds.h>

#include <utils/singleton_instance.h>
#include <utils/path.h>

#include <filesystem>
#include <gtest/gtest.h>

namespace kryga
{
struct base_test : public testing::Test
{
    void
    SetUp();

    void
    TearDown();

    kryga::utils::path
    get_current_workspace();

    kryga::singleton_registry m_regestry;
};
};  // namespace kryga