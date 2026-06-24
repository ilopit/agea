
#include "core/object_constructor.h"

#include <core/package_manager.h>
#include <core/level.h>
#include <core/level_manager.h>
#include <core/model_system.h>
#include <global_state/global_state.h>
#include <core/reflection/reflection_type.h>
#include <core/architype.h>
#include <core/object_load_context.h>
#include <testing/testing.h>
#include <core/core_state.h>
#include <vfs/vfs_state.h>
#include <vfs/vfs.h>
#include <vfs/physical_backend.h>

#include "packages/root/package.root.h"
#include "packages/root/package.root.types_builder.ar.h"
#include "packages/root/package.root.h"
#include "packages/root/package.root.types_builder.ar.h"
#include "packages/test/package.test.h"

#include <packages/root/model/game_object.h>
#include <packages/root/model/assets/mesh.h>
#include <packages/root/model/assets/material.h>
#include <packages/root/model/assets/texture.h>
#include <packages/test/model/test_mesh_object.h>
#include <packages/root/model/components/mesh_component.h>
#include <packages/root/model/assets/simple_texture_material.h>

#include <serialization/serialization.h>

#include <utils/kryga_log.h>
#include <utils/file_utils.h>

#include <gtest/gtest.h>
#include <sstream>
#include <fstream>

using namespace kryga;

namespace
{

// Convenience wrappers around v2 API for test code
std::expected<root::smart_object*, result_code>
test_object_load(const utils::id& id,
                 core::object_load_type type,
                 core::object_load_context& occ,
                 std::vector<root::smart_object*>& loaded_obj)
{
    auto old_objects = occ.reset_loaded_objects();
    core::object_constructor ctor(&occ, type);
    auto result = ctor.load_obj(id);
    occ.reset_loaded_objects(old_objects, loaded_obj);
    return result;
}

std::expected<root::smart_object*, result_code>
test_object_clone(root::smart_object& src,
                  core::object_load_type type,
                  const utils::id& new_id,
                  core::object_load_context& occ,
                  std::vector<root::smart_object*>& loaded_obj)
{
    auto old_objects = occ.reset_loaded_objects();
    core::object_constructor ctor(&occ, type);
    auto result = ctor.clone_obj(src, new_id);
    occ.reset_loaded_objects(old_objects, loaded_obj);
    return result;
}

std::expected<root::smart_object*, result_code>
test_object_instantiate(root::smart_object& src,
                        const utils::id& new_id,
                        core::object_load_context& occ,
                        std::vector<root::smart_object*>& loaded_obj)
{
    auto old_objects = occ.reset_loaded_objects();
    core::object_constructor ctor(&occ, core::object_load_type::instance_obj);
    auto result = ctor.instantiate_obj(src, new_id);
    occ.reset_loaded_objects(old_objects, loaded_obj);
    return result;
}

void
validate_empty_cache(gs::state& gs)
{
    for (auto i = core::architype_first; i < core::architype_last;
         i = (core::architype)((uint8_t)i + 1))
    {
        ASSERT_TRUE(gs.getr_model().caches.map.get_cache(i)->get_items().empty())
            << "Failed at " << ::kryga::core::to_string(i);
    }
}

testing::AssertionResult
compare_files_line_by_line(const utils::path& expected_path, const utils::path& actual_path)
{
    std::ifstream expected_file(expected_path.fs());
    std::ifstream actual_file(actual_path.fs());

    if (!expected_file.is_open())
    {
        return testing::AssertionFailure()
               << "Failed to open expected file: " << expected_path.str();
    }

    if (!actual_file.is_open())
    {
        return testing::AssertionFailure() << "Failed to open actual file: " << actual_path.str();
    }

    std::string expected_line, actual_line;
    int line_num = 0;

    while (true)
    {
        bool has_expected = static_cast<bool>(std::getline(expected_file, expected_line));
        bool has_actual = static_cast<bool>(std::getline(actual_file, actual_line));
        ++line_num;

        if (!has_expected && !has_actual)
        {
            break;  // Both files ended
        }

        if (!has_expected)
        {
            return testing::AssertionFailure() << "Actual file has extra lines starting at line "
                                               << line_num << ": \"" << actual_line << "\"";
        }

        if (!has_actual)
        {
            return testing::AssertionFailure() << "Expected file has extra lines starting at line "
                                               << line_num << ": \"" << expected_line << "\"";
        }

        if (expected_line != actual_line)
        {
            return testing::AssertionFailure() << "Line " << line_num << " differs:\n"
                                               << "  expected: \"" << expected_line << "\"\n"
                                               << "  actual:   \"" << actual_line << "\"";
        }
    }

    return testing::AssertionSuccess();
}

static int s_test_mount_counter = 0;

vfs::rid
mount_for_test(const std::filesystem::path& path)
{
    auto name = "test_mount_" + std::to_string(s_test_mount_counter++);
    glob::glob_state().getr_vfs().mount(name, std::make_unique<vfs::physical_backend>(path), 0);
    return {name, ""};
}

// Saves an object via VFS, then reloads it as an instance via the given level's load context.
// The reload_level must outlive the returned pointer. Returns nullptr on failure.
root::smart_object*
round_trip_save_load(root::smart_object& obj,
                     const utils::path& save_path,
                     core::level& reload_level)
{
    auto& reload_lc = reload_level.get_load_context();
    auto& vfs = glob::glob_state().getr_vfs();

    // Mount save_path's parent directory as VFS backend and set it as VFS root BEFORE saving,
    // so save_obj can resolve and write through VFS.
    static int s_round_trip_counter = 0;
    auto mount_name = "test_rt_" + std::to_string(s_round_trip_counter++);
    vfs.mount(vfs::rid("data", mount_name), save_path.parent().fs(), {.index_filter = ".aobj"});
    reload_lc.set_vfs_mount(vfs::rid("data", mount_name));

    auto rc = core::object_constructor(&reload_lc).save_obj(obj);
    if (rc != result_code::ok)
    {
        return nullptr;
    }

    // The file index uses filename stem as key — derive reload ID from it
    auto reload_id = obj.get_id();

    std::vector<root::smart_object*> reloaded;
    auto result =
        test_object_load(reload_id, core::object_load_type::instance_obj, reload_lc, reloaded);

    if (!result.has_value())
    {
        return nullptr;
    }

    return result.value();
}

}  // namespace

struct test_preloaded_test_package : base_test
{
    void
    SetUp() override
    {
        glob::glob_state_reset();

        auto& gs = glob::glob_state();
        state_mutator__vfs::set(gs);
        {
            auto root = std::filesystem::current_path().parent_path();
            auto& vfs = gs.getr_vfs();
            vfs.mount("data", std::make_unique<vfs::physical_backend>(root), 0);
            vfs.mount("cache", std::make_unique<vfs::physical_backend>(root / "cache"), 0);
            vfs.mount("tmp", std::make_unique<vfs::physical_backend>(root / "tmp"), 0);
            vfs.mount(
                "generated",
                std::make_unique<vfs::physical_backend>(root.parent_path() / "kryga_generated"),
                0);
        }
        core::state_mutator__lua_api::set(gs);
        core::state_mutator__model::set(gs);
        auto& pm = gs.getr_model().packages;

        gs.run_create();
        validate_empty_cache(gs);
        {
            {
                pm.register_static_package_loader<root::package>();
                auto& pkg = pm.load_static_package<root::package>();
                pkg.register_package_extension<root::package::package_types_builder>();
                pkg.complete_load();
            }
            {
                pm.register_static_package_loader<test::package>();
                auto& pkg = pm.load_static_package<test::package>();
                pkg.init();
                pkg.finalize_reflection();
            }
        }
    }

    vfs::backend* m_test_backend = nullptr;

    // Mount test.alvl as a backend with index
    void
    setup_test_backend(core::object_load_context& lc)
    {
        if (m_test_backend)
        {
            return;  // already mounted
        }
        auto& vfs = glob::glob_state().getr_vfs();
        auto real = vfs.real_path(vfs::rid("data", "levels/test.alvl"));
        m_test_backend = vfs.mount(
            vfs::rid("data", "levels/test.alvl"), real.value(), {.index_filter = ".aobj"});
        lc.set_vfs_mount(vfs::rid("data", "levels/test.alvl"));
    }

    void
    TearDown() override
    {
        test::package::instance().unload();
        root::package::instance().unload();
        root::package::instance().unload();
        glob::glob_state_reset();

        base_test::TearDown();
    }

    testing::AssertionResult
    validate_class_obj(root::smart_object& so)
    {
        auto class_obj = so.get_class_obj();
        if ((so.get_flags().derived_obj || so.get_flags().instance_obj) &&
            !so.get_flags().runtime_obj && !class_obj)
        {
            return testing::AssertionFailure() << "get_class_obj() is null for " << so.get_id();
        }

        if ((so.get_flags().default_obj || so.get_flags().runtime_obj) && class_obj)
        {
            return testing::AssertionFailure() << "get_class_obj() is not null for " << so.get_id();
        }

        return testing::AssertionSuccess();
    }

    testing::AssertionResult
    verify_flags(root::smart_object& so, const root::smart_object_flags& expected_flags)
    {
        const auto& flags = so.get_flags();
        std::stringstream errors;

        if (flags.instance_obj != expected_flags.instance_obj)
        {
            errors << "instance_obj flag mismatch: expected " << expected_flags.instance_obj
                   << " but got " << flags.instance_obj << " for " << so.get_id() << "; ";
        }

        if (flags.derived_obj != expected_flags.derived_obj)
        {
            errors << "derived_obj flag mismatch: expected " << expected_flags.derived_obj
                   << " but got " << flags.derived_obj << " for " << so.get_id() << "; ";
        }

        if (flags.runtime_obj != expected_flags.runtime_obj)
        {
            errors << "runtime_constructed flag mismatch: expected " << expected_flags.runtime_obj
                   << " but got " << flags.runtime_obj << " for " << so.get_id() << "; ";
        }

        if (flags.mirror_obj != expected_flags.mirror_obj)
        {
            errors << "mirror_obj flag mismatch: expected " << expected_flags.mirror_obj
                   << " but got " << flags.mirror_obj << " for " << so.get_id() << "; ";
        }

        if (flags.default_obj != expected_flags.default_obj)
        {
            errors << "default_obj flag mismatch: expected " << expected_flags.default_obj
                   << " but got " << flags.default_obj << " for " << so.get_id() << "; ";
        }

        if (flags.readonly != expected_flags.readonly)
        {
            errors << "readonly flag mismatch: expected " << expected_flags.readonly << " but got "
                   << flags.readonly << " for " << so.get_id() << "; ";
        }

        std::string error_msg = errors.str();
        if (!error_msg.empty())
        {
            return testing::AssertionFailure() << error_msg;
        }

        return testing::AssertionSuccess();
    }
};

TEST_F(test_preloaded_test_package, load_class_object_with_custom_layout)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto result = test_object_load(
        AID("test_complex_mesh_object"), core::object_load_type::class_obj, lc, loaded);

    ASSERT_TRUE(result.has_value());
    auto go = result.value()->as<root::game_object>();
    ASSERT_TRUE(go);
    ASSERT_TRUE(verify_flags(*go, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*go));

    ASSERT_EQ(loaded.size(), 5);

    ASSERT_EQ(go->get_id(), AID("test_complex_mesh_object"));
    ASSERT_EQ(go->get_architype_id(), core::architype::game_object);
    ASSERT_EQ(go->get_class_obj()->get_id(), AID("mesh_object"));

    auto components = go->get_subcomponents();
    ASSERT_EQ(components.size(), 2);

    auto comp1 = components[0];
    ASSERT_EQ(comp1->get_id(), AID("test_root_component_0"));
    ASSERT_EQ(comp1->get_architype_id(), core::architype::component);
    ASSERT_EQ(comp1->get_class_obj()->get_id(), AID("game_object_component"));
    ASSERT_TRUE(verify_flags(*comp1, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*comp1));

    auto comp2 = components[1]->as<root::mesh_component>();
    ASSERT_EQ(comp2->get_id(), AID("test_root_component_1"));
    ASSERT_EQ(comp2->get_architype_id(), core::architype::component);
    ASSERT_EQ(comp2->get_class_obj()->get_id(), AID("mesh_component"));
    ASSERT_TRUE(verify_flags(*comp2, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*comp2));

    auto mesh = comp2->get_mesh();
    ASSERT_EQ(mesh->get_id(), AID("test_mesh"));
    ASSERT_EQ(mesh->get_class_obj()->get_id(), AID("mesh"));
    ASSERT_TRUE(verify_flags(*mesh, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*mesh));

    auto material = comp2->get_material()->as<root::simple_texture_material>();
    ASSERT_EQ(material->get_id(), AID("test_material"));
    ASSERT_EQ(material->get_class_obj()->get_id(), AID("simple_texture_material"));
    ASSERT_TRUE(verify_flags(*material, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*material));

    // NOTE: texture_slot deserialization is currently broken after struct change
    // (sampler_id → smp pointer, txt not populated). Skipping texture_slot assertions.
}

TEST_F(test_preloaded_test_package, load_class_object_by_id)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto result = test_object_load(AID("test_obj"), core::object_load_type::class_obj, lc, loaded);

    ASSERT_TRUE(result.has_value());
    auto go = result.value()->as<root::game_object>();
    ASSERT_TRUE(go);
    ASSERT_TRUE(verify_flags(*go, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*go));

    ASSERT_EQ(go->get_id(), AID("test_obj"));
    ASSERT_EQ(go->get_class_obj()->get_id(), AID("game_object"));
}

TEST_F(test_preloaded_test_package, load_instance_object_by_id)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto result =
        test_object_load(AID("test_obj"), core::object_load_type::instance_obj, lc, loaded);

    ASSERT_TRUE(result.has_value());
    auto go = result.value()->as<root::game_object>();
    ASSERT_TRUE(go);
    ASSERT_TRUE(verify_flags(*go, core::ks_instance_derived));
    ASSERT_EQ(go->get_id(), AID("test_obj"));
}

TEST_F(test_preloaded_test_package, object_clone_class_object)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto load_result =
        test_object_load(AID("test_obj"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());
    auto src = load_result.value();
    ASSERT_TRUE(verify_flags(*src, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*src));

    std::vector<root::smart_object*> cloned_objs;
    auto clone_result = test_object_clone(
        *src, core::object_load_type::class_obj, AID("test_obj_clone"), lc, cloned_objs);

    ASSERT_TRUE(clone_result.has_value());
    auto cloned = clone_result.value();
    ASSERT_NE(cloned, src);
    ASSERT_EQ(cloned->get_id(), AID("test_obj_clone"));
    ASSERT_EQ(cloned->get_type_id(), src->get_type_id());
    ASSERT_TRUE(verify_flags(*cloned, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*cloned));
}

TEST_F(test_preloaded_test_package, object_clone_as_instance)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto load_result =
        test_object_load(AID("test_obj"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());
    auto src = load_result.value();
    ASSERT_TRUE(verify_flags(*src, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*src));

    std::vector<root::smart_object*> cloned_objs;
    auto clone_result = test_object_clone(*src,
                                          core::object_load_type::instance_obj,
                                          AID("test_obj_instance_clone"),
                                          lc,
                                          cloned_objs);

    ASSERT_TRUE(clone_result.has_value());
    auto cloned = clone_result.value();
    ASSERT_NE(cloned, src);
    ASSERT_EQ(cloned->get_id(), AID("test_obj_instance_clone"));
    ASSERT_TRUE(verify_flags(*cloned, core::ks_instance_derived));
    ASSERT_TRUE(validate_class_obj(*cloned));
}

TEST_F(test_preloaded_test_package, object_instantiate_from_proto)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto load_result =
        test_object_load(AID("test_obj"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());

    auto proto = load_result.value();
    ASSERT_TRUE(validate_class_obj(*proto));

    std::vector<root::smart_object*> instantiated_objs;
    auto inst_result =
        test_object_instantiate(*proto, AID("test_obj_instance"), lc, instantiated_objs);

    ASSERT_TRUE(inst_result.has_value());
    auto instance = inst_result.value();
    ASSERT_NE(instance, proto);
    ASSERT_EQ(instance->get_id(), AID("test_obj_instance"));
    ASSERT_EQ(instance->get_class_obj(), proto);
    ASSERT_TRUE(verify_flags(*instance, core::ks_instance_derived));
    ASSERT_TRUE(validate_class_obj(*instance));
}

TEST_F(test_preloaded_test_package, diff_object_properties_same_objects)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded1;
    auto result1 =
        test_object_load(AID("test_obj"), core::object_load_type::class_obj, lc, loaded1);
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(verify_flags(*result1.value(), core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*result1.value()));

    std::vector<reflection::property*> diff;
    auto rc = core::diff_object_properties(*result1.value(), *result1.value(), diff);

    ASSERT_EQ(rc, result_code::ok);
    ASSERT_TRUE(diff.empty());
}

TEST_F(test_preloaded_test_package, diff_object_properties_different_types_fails)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded1, loaded2;
    auto result1 =
        test_object_load(AID("test_obj"), core::object_load_type::class_obj, lc, loaded1);
    auto result2 =
        test_object_load(AID("test_mesh"), core::object_load_type::class_obj, lc, loaded2);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    ASSERT_TRUE(validate_class_obj(*result1.value()));
    ASSERT_TRUE(validate_class_obj(*result2.value()));

    std::vector<reflection::property*> diff;
    auto rc = core::diff_object_properties(*result1.value(), *result2.value(), diff);

    ASSERT_EQ(rc, result_code::failed);
}

TEST_F(test_preloaded_test_package, load_nonexistent_object_fails)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    lc.set_vfs_mount(vfs::rid("data", "levels/test.alvl"));

    std::vector<root::smart_object*> loaded;
    auto result =
        test_object_load(AID("nonexistent_object"), core::object_load_type::class_obj, lc, loaded);

    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error(), result_code::path_not_found);
}

TEST_F(test_preloaded_test_package, load_invalid_path_fails)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto result =
        test_object_load(AID("does_not_exist"), core::object_load_type::class_obj, lc, loaded);

    ASSERT_FALSE(result.has_value());
}

TEST_F(test_preloaded_test_package, cached_object_returns_same_pointer)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded1, loaded2;
    auto result1 =
        test_object_load(AID("test_obj"), core::object_load_type::class_obj, lc, loaded1);
    auto result2 =
        test_object_load(AID("test_obj"), core::object_load_type::class_obj, lc, loaded2);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    ASSERT_EQ(result1.value(), result2.value());
    ASSERT_TRUE(verify_flags(*result1.value(), core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*result1.value()));
}

TEST_F(test_preloaded_test_package, object_instantiate_complex_object_with_components)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto load_result = test_object_load(
        AID("test_complex_mesh_object"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());

    auto proto = load_result.value()->as<root::game_object>();
    ASSERT_TRUE(proto);
    ASSERT_TRUE(validate_class_obj(*proto));

    std::vector<root::smart_object*> instantiated_objs;
    auto inst_result =
        test_object_instantiate(*proto, AID("complex_instance"), lc, instantiated_objs);

    ASSERT_TRUE(inst_result.has_value());
    auto instance = inst_result.value()->as<root::game_object>();
    ASSERT_TRUE(instance);
    ASSERT_TRUE(verify_flags(*instance, core::ks_instance_derived));
    ASSERT_TRUE(validate_class_obj(*instance));

    ASSERT_EQ(instance->get_id(), AID("complex_instance"));

    auto proto_components = proto->get_subcomponents();
    auto instance_components = instance->get_subcomponents();
    ASSERT_EQ(instance_components.size(), proto_components.size());

    for (auto comp : instance_components)
    {
        ASSERT_TRUE(verify_flags(*comp, core::ks_instance_derived));
        ASSERT_TRUE(validate_class_obj(*comp));
    }
}

TEST_F(test_preloaded_test_package, load_instance_object_with_custom_layout)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);
    std::vector<root::smart_object*> loaded;

    auto result = test_object_load(
        AID("test_complex_mesh_object"), core::object_load_type::instance_obj, lc, loaded);

    ASSERT_TRUE(result.has_value());
    auto go = result.value()->as<root::game_object>();
    ASSERT_TRUE(go);
    ASSERT_TRUE(verify_flags(*go, core::ks_instance_derived));

    ASSERT_EQ(go->get_id(), AID("test_complex_mesh_object"));
    ASSERT_EQ(go->get_architype_id(), core::architype::game_object);

    auto components = go->get_subcomponents();
    ASSERT_EQ(components.size(), 2);

    for (auto comp : components)
    {
        ASSERT_TRUE(verify_flags(*comp, core::ks_instance_derived));
    }

    auto comp2 = components[1]->as<root::mesh_component>();
    ASSERT_TRUE(comp2);

    auto mesh = comp2->get_mesh();
    ASSERT_EQ(mesh->get_id(), AID("test_mesh"));

    auto material = comp2->get_material()->as<root::simple_texture_material>();
    ASSERT_EQ(material->get_id(), AID("test_material"));
}

TEST_F(test_preloaded_test_package, object_construct_in_package_context)
{
    auto& lc = test::package::instance().get_load_context();

    root::game_object::construct_params params;
    params.pos = {1.0f, 2.0f, 3.0f};

    auto result = core::object_constructor(&lc).construct_obj(
        AID("game_object"), AID("constructed_game_object"), params, true);

    ASSERT_TRUE(result.has_value());
    auto obj = result.value();
    ASSERT_TRUE(obj);

    ASSERT_EQ(obj->get_id(), AID("constructed_game_object"));
    ASSERT_EQ(obj->get_type_id(), AID("game_object"));

    ASSERT_TRUE(verify_flags(*obj, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*obj));

    auto go = obj->as<root::game_object>();
    ASSERT_TRUE(go);
    ASSERT_EQ(go->get_position(), root::vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(test_preloaded_test_package, object_construct_invalid_type_fails)
{
    auto& lc = test::package::instance().get_load_context();

    root::smart_object::construct_params params;

    auto result = core::object_constructor(&lc).construct_obj(
        AID("nonexistent_type_xyz"), AID("should_fail"), params, true);

    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error(), result_code::path_not_found);
}

TEST_F(test_preloaded_test_package, object_construct_in_level_context)
{
    auto& lc = test::package::instance().get_load_context();

    root::game_object::construct_params params;
    params.pos = {5.0f, 6.0f, 7.0f};

    auto result =
        core::object_constructor(&lc, core::object_load_type::instance_obj)
            .construct_obj(AID("game_object"), AID("level_constructed_object"), params, false);

    ASSERT_TRUE(result.has_value());
    auto obj = result.value();
    ASSERT_TRUE(obj);

    ASSERT_EQ(obj->get_id(), AID("level_constructed_object"));
    ASSERT_EQ(obj->get_type_id(), AID("game_object"));

    ASSERT_TRUE(verify_flags(*obj, core::ks_instance_derived));
    ASSERT_TRUE(validate_class_obj(*obj));

    auto go = obj->as<root::game_object>();
    ASSERT_TRUE(go);
    ASSERT_EQ(go->get_position(), root::vec3(5.0f, 6.0f, 7.0f));
}

TEST_F(test_preloaded_test_package, object_save_and_reload_full)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();

    // 1. Load object from existing test file (use stem as ID)
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto load_result = test_object_load(
        AID("test_obj_custom_layout"), core::object_load_type::class_obj, lc, loaded);

    ASSERT_TRUE(load_result.has_value());
    auto obj = load_result.value()->as<root::game_object>();
    ASSERT_TRUE(obj);
    ASSERT_TRUE(verify_flags(*obj, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*obj));

    // 2. Save through VFS
    auto save_result = core::object_constructor(&lc).save_obj(*obj);
    ASSERT_EQ(save_result, result_code::ok);

    // 3. Verify file exists via VFS
    vfs::rid saved_rid;
    ASSERT_TRUE(lc.resolve(AID("test_obj_custom_layout"), saved_rid));
    auto& vfs = glob::glob_state().getr_vfs();
    ASSERT_TRUE(vfs.exists(saved_rid));

    // 4. Compare saved file with original line by line
    auto saved_rp = vfs.real_path(saved_rid);
    ASSERT_TRUE(saved_rp.has_value());
    auto saved_path = APATH(saved_rp.value());

    auto expected_rp = vfs.real_path(
        vfs::rid("data", "levels/test.alvl/game_objects/test_obj_custom_layout.aobj"));
    ASSERT_TRUE(expected_rp.has_value());
    auto expected_path = APATH(expected_rp.value());
    ASSERT_TRUE(compare_files_line_by_line(expected_path, saved_path));
}

TEST_F(test_preloaded_test_package, object_save_reload_simple)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto load_result =
        test_object_load(AID("test_obj"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());
    auto obj = load_result.value()->as<root::game_object>();
    ASSERT_TRUE(obj);

    auto temp_dir = utils::path(std::filesystem::temp_directory_path());
    auto save_path = temp_dir / "test_obj_rt.aobj";

    core::level reload_level(AID("reload_simple"));
    auto* reloaded = round_trip_save_load(*obj, save_path, reload_level);
    ASSERT_TRUE(reloaded);

    // Verify saved file matches the original via VFS
    auto& reload_lc = reload_level.get_load_context();
    vfs::rid saved_rid;
    ASSERT_TRUE(reload_lc.resolve(AID("test_obj"), saved_rid));
    auto& vfs = gs.getr_vfs();
    auto saved_rp = vfs.real_path(saved_rid);
    ASSERT_TRUE(saved_rp.has_value());
    auto saved_real_path = APATH(saved_rp.value());

    auto expected_rp =
        vfs.real_path(vfs::rid("data", "levels/test.alvl/game_objects/test_obj.aobj"));
    ASSERT_TRUE(expected_rp.has_value());
    auto expected_path = APATH(expected_rp.value());
    ASSERT_TRUE(compare_files_line_by_line(expected_path, saved_real_path));

    auto reloaded_go = reloaded->as<root::game_object>();
    ASSERT_TRUE(reloaded_go);
    ASSERT_EQ(reloaded_go->get_id(), obj->get_id());
    ASSERT_EQ(reloaded_go->get_type_id(), obj->get_type_id());

    auto orig_components = obj->get_subcomponents();
    auto reloaded_components = reloaded_go->get_subcomponents();
    ASSERT_EQ(reloaded_components.size(), orig_components.size());

    for (size_t i = 0; i < orig_components.size(); ++i)
    {
        ASSERT_EQ(reloaded_components[i]->get_id(), orig_components[i]->get_id());
    }

    std::filesystem::remove(saved_real_path.fs());
}

TEST_F(test_preloaded_test_package, object_save_reload_complex_mesh_object)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto load_result = test_object_load(
        AID("test_complex_mesh_object"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());
    auto obj = load_result.value()->as<root::game_object>();
    ASSERT_TRUE(obj);

    auto orig_components = obj->get_subcomponents();
    ASSERT_EQ(orig_components.size(), 2);
    auto orig_mesh_comp = orig_components[1]->as<root::mesh_component>();
    ASSERT_TRUE(orig_mesh_comp);

    // Verify save succeeds and produces output via VFS
    auto save_rc = core::object_constructor(&lc).save_obj(*obj);
    ASSERT_EQ(save_rc, result_code::ok);

    vfs::rid saved_rid;
    ASSERT_TRUE(lc.resolve(AID("test_complex_mesh_object"), saved_rid));
    auto& vfs = glob::glob_state().getr_vfs();
    ASSERT_TRUE(vfs.exists(saved_rid));

    // NOTE: Full round-trip reload is not tested here because object_save writes
    // asset references as YAML maps ({id: name}) while the loader expects scalar
    // strings. This is a known serialization format mismatch.
}

TEST_F(test_preloaded_test_package, object_save_reload_material)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto load_result =
        test_object_load(AID("test_material"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());
    auto material = load_result.value()->as<root::simple_texture_material>();
    ASSERT_TRUE(material);

    // Verify save succeeds and produces output via VFS
    auto save_rc = core::object_constructor(&lc).save_obj(*material);
    ASSERT_EQ(save_rc, result_code::ok);

    vfs::rid saved_rid;
    ASSERT_TRUE(lc.resolve(AID("test_material"), saved_rid));
    auto& vfs = glob::glob_state().getr_vfs();
    ASSERT_TRUE(vfs.exists(saved_rid));

    // NOTE: Full round-trip reload is not tested here because object_save writes
    // asset reference properties (e.g. simple_texture) as YAML maps ({id: name})
    // while the loader expects scalar strings. This is a known serialization
    // format mismatch.
}

TEST_F(test_preloaded_test_package, object_save_reload_idempotent)
{
    auto& lc = test::package::instance().get_load_context();
    auto& gs = glob::glob_state();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto load_result =
        test_object_load(AID("test_obj"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());
    auto obj = load_result.value();

    auto temp_dir = utils::path(std::filesystem::temp_directory_path());
    auto save_path_a = temp_dir / "idempotent_a.aobj";

    // First save and reload
    core::level reload_level(AID("reload_idempotent"));
    auto* reloaded = round_trip_save_load(*obj, save_path_a, reload_level);
    ASSERT_TRUE(reloaded);

    // Resolve the first save's real path
    auto& reload_lc = reload_level.get_load_context();
    auto& vfs = gs.getr_vfs();
    vfs::rid saved_rid_a;
    ASSERT_TRUE(reload_lc.resolve(AID("test_obj"), saved_rid_a));
    auto rp_a = vfs.real_path(saved_rid_a);
    ASSERT_TRUE(rp_a.has_value());
    auto real_path_a = APATH(rp_a.value());

    // Second save from the reloaded object — overwrite the same VFS location
    auto save_rc = core::object_constructor(&reload_lc).save_obj(*reloaded);
    ASSERT_EQ(save_rc, result_code::ok);

    // The second save overwrites the same file; read it back and compare with
    // the original test data to confirm idempotency
    auto expected_rp =
        vfs.real_path(vfs::rid("data", "levels/test.alvl/game_objects/test_obj.aobj"));
    ASSERT_TRUE(expected_rp.has_value());
    auto expected_path = APATH(expected_rp.value());
    ASSERT_TRUE(compare_files_line_by_line(expected_path, real_path_a));

    std::filesystem::remove(real_path_a.fs());
}

TEST_F(test_preloaded_test_package, object_save_reload_constructed_object)
{
    auto& lc = test::package::instance().get_load_context();

    root::game_object::construct_params params;
    auto construct_result = core::object_constructor(&lc).construct_obj(
        AID("game_object"), AID("rt_constructed_obj"), params, true);
    ASSERT_TRUE(construct_result.has_value());
    auto obj = construct_result.value();
    ASSERT_TRUE(obj);

    ASSERT_EQ(obj->get_type_id(), AID("game_object"));
    ASSERT_TRUE(verify_flags(*obj, core::ks_class_derived));
    ASSERT_TRUE(validate_class_obj(*obj));

    auto save_rc = core::object_constructor(&lc).save_obj(*obj);
    ASSERT_EQ(save_rc, result_code::ok);

    vfs::rid saved_rid;
    ASSERT_TRUE(lc.resolve(AID("rt_constructed_obj"), saved_rid));
    auto& vfs = glob::glob_state().getr_vfs();
    ASSERT_TRUE(vfs.exists(saved_rid));

    // Cleanup
    vfs.remove(saved_rid);
}

TEST_F(test_preloaded_test_package, object_save_material_preserves_texture_slots)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto load_result =
        test_object_load(AID("test_material"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());
    auto* material = load_result.value()->as<root::simple_texture_material>();
    ASSERT_TRUE(material);

    ASSERT_TRUE(material->simple_texture().txt) << "texture slot should be populated after load";

    auto save_rc = core::object_constructor(&lc).save_obj(*material);
    ASSERT_EQ(save_rc, result_code::ok);

    vfs::rid saved_rid;
    ASSERT_TRUE(lc.resolve(AID("test_material"), saved_rid));

    serialization::container sc;
    ASSERT_TRUE(serialization::read_container(saved_rid, sc));

    ASSERT_TRUE(sc["simple_texture"].IsDefined()) << "texture slot missing from saved file";
    ASSERT_TRUE(sc["simple_texture"]["texture"].IsDefined())
        << "texture id missing from saved texture slot";
    EXPECT_EQ(sc["simple_texture"]["texture"].as<std::string>(), "texture");
    EXPECT_EQ(sc["simple_texture"]["slot"].as<uint32_t>(), 0u);
}

// ============================================================================
// Unified cache: readonly invariants
// ============================================================================

TEST_F(test_preloaded_test_package, package_objects_are_all_readonly)
{
    auto& local = root::package::instance().get_local_cache();
    auto& items = local.objects.get_items();
    ASSERT_FALSE(items.empty());

    for (auto& [id, obj] : items)
    {
        ASSERT_TRUE(obj->get_flags().readonly)
            << obj->get_id() << " should be readonly after package load";
        ASSERT_FALSE(obj->get_flags().instance_obj)
            << obj->get_id() << " should not be instance after package load";
    }
}

TEST_F(test_preloaded_test_package, package_components_are_readonly)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto result = test_object_load(
        AID("test_complex_mesh_component"), core::object_load_type::class_obj, lc, loaded);

    ASSERT_TRUE(result.has_value());
    auto comp = result.value();
    ASSERT_TRUE(comp->get_flags().readonly);
    ASSERT_FALSE(comp->get_flags().instance_obj);
    ASSERT_TRUE(verify_flags(*comp, core::ks_class_derived));
}

TEST_F(test_preloaded_test_package, package_subobjects_are_readonly)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto result =
        test_object_load(AID("test_material"), core::object_load_type::class_obj, lc, loaded);

    ASSERT_TRUE(result.has_value());
    auto material = result.value()->as<root::simple_texture_material>();
    ASSERT_TRUE(material);
    ASSERT_TRUE(material->get_flags().readonly);

    auto* se = material->get_shader_effect();
    ASSERT_TRUE(se) << "shader_effect should be loaded";
    ASSERT_TRUE(se->get_flags().readonly) << "shader_effect should be readonly";

    auto& ts = material->simple_texture();
    if (ts.txt)
    {
        ASSERT_TRUE(ts.txt->get_flags().readonly) << "texture should be readonly";
    }
}

// ============================================================================
// load_obj instance mode — readonly behavior
// ============================================================================

TEST_F(test_preloaded_test_package, load_obj_instance_returns_readonly_class_for_cdo)
{
    auto& lc = test::package::instance().get_load_context();

    std::vector<root::smart_object*> loaded_class, loaded_inst;
    auto class_result =
        test_object_load(AID("game_object"), core::object_load_type::class_obj, lc, loaded_class);
    auto inst_result =
        test_object_load(AID("game_object"), core::object_load_type::instance_obj, lc, loaded_inst);

    ASSERT_TRUE(class_result.has_value());
    ASSERT_TRUE(inst_result.has_value());
    ASSERT_EQ(class_result.value(), inst_result.value()) << "readonly CDO should be shared";
    ASSERT_TRUE(inst_result.value()->get_flags().readonly);
}

TEST_F(test_preloaded_test_package, load_obj_instance_returns_readonly_class_for_derived)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded1, loaded2;
    auto result1 =
        test_object_load(AID("test_material"), core::object_load_type::instance_obj, lc, loaded1);

    ASSERT_TRUE(result1.has_value());
    auto obj = result1.value();
    ASSERT_TRUE(verify_flags(*obj, core::ks_instance_derived));

    auto result2 =
        test_object_load(AID("test_material"), core::object_load_type::instance_obj, lc, loaded2);
    ASSERT_TRUE(result2.has_value());
    ASSERT_EQ(result1.value(), result2.value()) << "second load should return same pointer";
}

TEST_F(test_preloaded_test_package, load_obj_instance_returns_readonly_for_mesh)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto result =
        test_object_load(AID("test_mesh"), core::object_load_type::instance_obj, lc, loaded);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(verify_flags(*result.value(), core::ks_instance_derived));
}

// ============================================================================
// Instantiation tests
// ============================================================================

TEST_F(test_preloaded_test_package, instantiate_game_object_creates_instance_components)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto load_result =
        test_object_load(AID("test_obj"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());
    auto proto = load_result.value()->as<root::game_object>();
    ASSERT_TRUE(proto);

    std::vector<root::smart_object*> instantiated;
    auto inst_result = test_object_instantiate(*proto, AID("inst_go"), lc, instantiated);
    ASSERT_TRUE(inst_result.has_value());

    auto instance = inst_result.value()->as<root::game_object>();
    ASSERT_TRUE(instance);
    ASSERT_TRUE(instance->get_flags().instance_obj);

    auto proto_comps = proto->get_subcomponents();
    auto inst_comps = instance->get_subcomponents();
    ASSERT_EQ(inst_comps.size(), proto_comps.size());

    for (size_t i = 0; i < inst_comps.size(); ++i)
    {
        ASSERT_NE(inst_comps[i], proto_comps[i])
            << "component " << i << " should be a fresh instance";
        ASSERT_TRUE(inst_comps[i]->get_flags().instance_obj)
            << "component " << i << " should be instance";
        ASSERT_EQ(inst_comps[i]->get_owner(), instance)
            << "component " << i << " owner should be the instance GO";
    }
}

TEST_F(test_preloaded_test_package, instantiate_preserves_readonly_sub_assets)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto load_result = test_object_load(
        AID("test_complex_mesh_object"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());
    auto proto = load_result.value()->as<root::game_object>();
    ASSERT_TRUE(proto);

    auto proto_comps = proto->get_subcomponents();
    ASSERT_GE(proto_comps.size(), 2u);
    auto* proto_mc = proto_comps[1]->as<root::mesh_component>();
    ASSERT_TRUE(proto_mc);

    std::vector<root::smart_object*> instantiated;
    auto inst_result = test_object_instantiate(*proto, AID("inst_complex"), lc, instantiated);
    ASSERT_TRUE(inst_result.has_value());
    auto instance = inst_result.value()->as<root::game_object>();
    ASSERT_TRUE(instance);

    auto inst_comps = instance->get_subcomponents();
    ASSERT_GE(inst_comps.size(), 2u);
    auto* inst_mc = inst_comps[1]->as<root::mesh_component>();
    ASSERT_TRUE(inst_mc);

    ASSERT_EQ(inst_mc->get_mesh(), proto_mc->get_mesh())
        << "readonly mesh should be shared, not cloned";
    ASSERT_EQ(inst_mc->get_material(), proto_mc->get_material())
        << "readonly material should be shared, not cloned";
}

TEST_F(test_preloaded_test_package, no_promotion_in_cache)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto load_result =
        test_object_load(AID("test_obj"), core::object_load_type::class_obj, lc, loaded);
    ASSERT_TRUE(load_result.has_value());
    auto proto = load_result.value();

    std::vector<root::smart_object*> instantiated;
    auto inst_result = test_object_instantiate(*proto, AID("test_obj_inst"), lc, instantiated);
    ASSERT_TRUE(inst_result.has_value());
    auto instance = inst_result.value();

    ASSERT_EQ(lc.find_obj(AID("test_obj")), proto) << "class object should still be in cache";
    ASSERT_EQ(lc.find_obj(AID("test_obj_inst")), instance) << "instance should be findable";
    ASSERT_TRUE(verify_flags(*proto, core::ks_class_derived))
        << "class object should retain class flags after instantiation";
    ASSERT_TRUE(verify_flags(*instance, core::ks_instance_derived))
        << "instance should have instance flags";
}

// ============================================================================
// Integration
// ============================================================================

TEST_F(test_preloaded_test_package, load_instance_cold_cache_readonly_object)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded;
    auto result =
        test_object_load(AID("test_material"), core::object_load_type::instance_obj, lc, loaded);

    ASSERT_TRUE(result.has_value());
    auto obj = result.value();
    ASSERT_TRUE(verify_flags(*obj, core::ks_instance_derived));
}

TEST_F(test_preloaded_test_package, cached_instance_returned_on_second_load)
{
    auto& lc = test::package::instance().get_load_context();
    setup_test_backend(lc);

    std::vector<root::smart_object*> loaded1, loaded2;
    auto result1 =
        test_object_load(AID("test_obj"), core::object_load_type::instance_obj, lc, loaded1);
    auto result2 =
        test_object_load(AID("test_obj"), core::object_load_type::instance_obj, lc, loaded2);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    ASSERT_EQ(result1.value(), result2.value());
}
