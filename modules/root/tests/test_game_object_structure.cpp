#include "base_test.h"

#include <model/id_generator.h>
#include <model/caches/objects_cache.h>
#include <model/caches/objects_cache.h>
#include <model/caches/caches_map.h>
#include <root/game_object.h>
#include <model/object_constructor.h>

#include <utils/singleton_registry.h>

#include <gtest/gtest.h>

using namespace agea;
using namespace core;
using namespace root;

struct test_game_object_structure : base_test
{
    void
    SetUp()
    {
        base_test::SetUp();
        glob::id_generator::create(m_reg);
        glob::class_objects_cache::create_ref(&m_class_cache);
        glob::objects_cache::create_ref(&m_cache);
    }

    singleton_registry m_reg;
    core::objects_cache m_class_cache;
    core::objects_cache m_cache;
};

template <typename T>
auto
aeo(const utils::id& id)
{
    return core::object_constructor::alloc_empty_object<T>(id);
}

TEST_F(test_game_object_structure, generate_from_ids)
{
    using namespace core;

    auto go = aeo<game_object>(AID("game_object"));
    auto c0 = aeo<game_object_component>(AID("c0"));
    auto c1 = aeo<game_object_component>(AID("c1"));
    auto c2 = aeo<game_object_component>(AID("c2"));
    auto c3 = aeo<game_object_component>(AID("c3"));
    auto c4 = aeo<game_object_component>(AID("c4"));
    auto c5 = aeo<game_object_component>(AID("c5"));

    /*
     0 - 1
       - 2  - 3
            - 4 - 5
    */

    c0->set_order_parent_idx(0, -1);
    c1->set_order_parent_idx(1, 0);
    c2->set_order_parent_idx(2, 0);
    c3->set_order_parent_idx(3, 2);
    c4->set_order_parent_idx(4, 2);
    c5->set_order_parent_idx(5, 4);

    go->attach(c1.get());
    go->attach(c3.get());
    go->attach(c0.get());
    go->attach(c2.get());
    go->attach(c5.get());
    go->attach(c4.get());

    go->recreate_structure_from_ids();

    for (auto& c : go->get_components())
    {
        std::cout << c.get_id() << std::endl;
    }
}

TEST_F(test_game_object_structure, generate_from_structure)
{
    using namespace core;

    auto go = aeo<game_object>(AID("game_object"));
    auto c0 = aeo<game_object_component>(AID("c0"));
    auto c1 = aeo<game_object_component>(AID("c1"));
    auto c2 = aeo<game_object_component>(AID("c2"));
    auto c3 = aeo<game_object_component>(AID("c3"));
    auto c4 = aeo<game_object_component>(AID("c4"));
    auto c5 = aeo<game_object_component>(AID("c5"));

    /*
     0 - 1
       - 2  - 3
            - 4 - 5
    */
    c0->add_child(c1.get()).add_child(c2.get());
    c2->add_child(c3.get()).add_child(c4.get());
    c4->add_child(c5.get());

    go->set_root_component(c0.get());

    go->recreate_structure_from_layout();
}
