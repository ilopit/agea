#include "model/object_constructor.h"

#include "model/caches/materials_cache.h"
#include "model/caches/textures_cache.h"
#include "model/caches/meshes_cache.h"
#include "model/caches/components_cache.h"
#include "model/caches/objects_cache.h"
#include "model/caches/game_objects_cache.h"
#include "model/caches/empty_objects_cache.h"
#include "model/caches/cache_set.h"
#include "model/reflection/lua_api.h"

#include "model/object_construction_context.h"
#include "model/level.h"
#include "model/package.h"
#include "model/game_object.h"

#include "utils/agea_log.h"
#include "utils/file_utils.h"
#include "utils/path.h"

#include "base_test.h"

#define ID(val) ::agea::utils::id::from(val)
using namespace agea;

struct test_luad_binding : public base_test
{
    void
    SetUp()
    {
        base_test::SetUp();
        glob::init_global_caches(m_class_objects_cache_set, m_objects_cache_set);
    }

    void
    TearDown()
    {
        base_test::TearDown();
    }

    agea::singletone_autodeleter m_objects_cache_set;
    agea::singletone_autodeleter m_class_objects_cache_set;
};

model::smart_object*
item_aa()
{
    auto itr = glob::class_components_cache::get()->get_items().begin();
    return itr->second;
}

model::smart_object*
item_bb(const char*)
{
    auto itr = glob::class_components_cache::get()->get_items().begin();
    return itr->second;
}

struct player
{
public:
    int bullets;
    int speed;

    player()
        : player(3, 100)
    {
    }

    player(int ammo)
        : player(ammo, 100)
    {
    }

    player(int ammo, int hitpoints)
        : bullets(ammo)
        , hp(hitpoints)
    {
    }

private:
    int hp;
};

TEST_F(test_luad_binding, load_package)
{
    sol::state lua;

    sol::usertype<utils::id> aa = lua.new_usertype<utils::id>("id",
                                                              // 3 constructors
                                                              sol::constructors<utils::id()>());

    auto path = glob::resource_locator::get()->resource(category::packages, "test.apkg");

    model::package p;
    auto result =
        model::package::load_package(path, p, glob::class_objects_cache_set::getr().get_ref(),
                                     glob::objects_cache_set::getr().get_ref());

    p.propagate_to_global_caches();

    ASSERT_TRUE(result);

    lua.open_libraries(sol::lib::base);

    using namespace model;

    // make usertype metatable
    sol::usertype<smart_object> player_type = lua.new_usertype<smart_object>(
        "smart_object", sol::meta_function::construct,
        sol::factories(
            // MyClass.new(...) -- dot syntax, no "self" value
            // passed in

            [](sol::object, const int d)
            {
                auto itr = glob::class_components_cache::get()->get_items().begin();
                return itr->second;
            }),
        sol::call_constructor,
        sol::factories(
            [](const int& d)
            {
                auto itr = glob::class_components_cache::get()->get_items().begin();
                return itr->second;
            })

    );

    // .set(foo, bar) is the same as [foo] = bar;
    player_type["id"] = &smart_object::get_id;

    // You can also add members to the code, defined in Lua!
    // This lets you have a high degree of flexibility in the
    // code

    std::string player_script = R"(
 obj = smart_object:new(2)
 print(obj:id())
 obj = smart_object(2)

)";

    lua.script(player_script);
}
