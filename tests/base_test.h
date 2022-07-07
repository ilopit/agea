#pragma once

#include "core/agea_minimal.h"

#include "model/model_fwds.h"
#include "core/fs_locator.h"
#include "utils/weird_singletone.h"

#include <filesystem>
#include <gtest/gtest.h>

struct base_test : public testing::Test
{
    void
    SetUp();

    void
    TearDown();

    agea::utils::path
    get_current_workspace();

    std::unique_ptr<agea::closure<agea::resource_locator>> m_resource_locator;
};