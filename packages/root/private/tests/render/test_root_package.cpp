
#include "core/object_constructor.h"

#include <core/package_manager.h>
#include <core/level.h>
#include <core/level_manager.h>
#include <global_state/global_state.h>
#include <core/reflection/reflection_type.h>
#include <core/architype.h>
#include <core/object_load_context.h>
#include <testing/testing.h>
#include <core/core_state.h>
#include <resource_locator/resource_locator_state.h>

#include "packages/root/package.root.h"

#include <packages/root/model/game_object.h>
#include <packages/root/model/assets/mesh.h>
#include <packages/root/model/assets/material.h>
#include <packages/root/model/assets/texture.h>

#include <utils/agea_log.h>
#include <utils/file_utils.h>

#include <gtest/gtest.h>

using namespace agea;

// extern int blabla;

struct test_preloaded_test_package : base_test
{
};

TEST_F(test_preloaded_test_package, load_class_object_with_custom_layout)
{
    auto& gs = glob::glob_state();
    gs.run_create();
    gs.run_connect();

    // auto a = blabla;
}