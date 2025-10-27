
#include "core/object_constructor.h"

#include <core/package_manager.h>
#include <core/level.h>
#include <core/level_manager.h>
#include <core/global_state.h>
#include <core/reflection/reflection_type.h>
#include <core/architype.h>

#include "packages/root/package.root.h"

#include <utils/agea_log.h>
#include <utils/file_utils.h>

#include <gtest/gtest.h>

#include "base_test.h"

using namespace agea;

void
validate_empty_cache(core::state& gs)
{
    for (auto i = core::architype::first; i < core::architype::last;
         i = (core::architype)((uint8_t)i + 1))
    {
        ASSERT_TRUE(gs.getr_class_cache_map().get_cache(i)->get_items().empty())
            << "Failed at " << ::agea::core::to_string(i);
    }
}

struct test_root_package : public base_test
{
    void
    SetUp()
    {
        base_test::SetUp();

        agea::glob::state::reset();
    }

    void
    TearDown()
    {
        root::package::reset_instance();
        agea::glob::state::reset();

        base_test::TearDown();
    }
};

TEST_F(test_root_package, basic_load)
{
    auto& gs = glob::state::getr();

    ///
    gs.schedule_action(core::state::state_stage::create,
                       [](core::state& s)
                       {
                           // state
                           core::state_mutator__caches::set(s);
                           core::state_mutator__level_manager::set(s);
                           core::state_mutator__package_manager::set(s);
                           core::state_mutator__reflection_manager::set(s);
                           core::state_mutator__id_generator::set(s);
                           core::state_mutator__lua_api::set(s);
                       });
    gs.run_create();

    root::package::init_instance();
    auto& pkg = root::package::instance();
    pkg.register_package_extention<root::package::package_types_builder>();
    pkg.register_package_extention<root::package::package_types_default_objects_builder>();

    validate_empty_cache(gs);

    pkg.init();

    pkg.load_types();
    pkg.load_render_types();
    pkg.load_render_resources();
    pkg.finalize_relfection();
    pkg.create_default_types_objects();

    ASSERT_EQ(pkg.get_reflection_types().size(), 38);
    auto s = pkg.get_local_cache().objects->get_size();

    pkg.destroy_default_types_objects();
    pkg.destroy_render_resources();
    pkg.destroy_render_types();
    pkg.destroy_types();

    ASSERT_TRUE(gs.getr_rm().get_types_to_id().empty());
    ASSERT_TRUE(gs.getr_rm().get_types_to_name().empty());
    validate_empty_cache(gs);
}
