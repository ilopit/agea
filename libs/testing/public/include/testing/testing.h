#pragma once

#include <utils/path.h>
#include <utils/id.h>

#include <filesystem>
#include <gtest/gtest.h>
#include <iostream>

namespace agea
{

struct testing_base
{
    void
    Do_SetUp();

    void
    Do_TearDown();

    agea::utils::path
    get_current_workspace();
};

struct base_test : public testing_base, public testing::Test
{
    void
    SetUp()
    {
        Do_SetUp();
    }

    void
    TearDown()
    {
        Do_TearDown();
    }
};

template <typename T>
struct base_test_with_params : public testing_base, public ::testing::TestWithParam<T>
{
    void
    SetUp()
    {
        Do_SetUp();
    }

    void
    TearDown()
    {
        Do_TearDown();
    }
};

namespace utils
{
inline void
PrintTo(const id& p, std::ostream* os)
{
    *os << p.cstr();
}

}  // namespace utils

};  // namespace agea