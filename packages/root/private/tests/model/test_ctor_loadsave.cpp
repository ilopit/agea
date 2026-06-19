#include "test_ctor_fixture.h"

// ============================================================================
// save / load roundtrip
// ============================================================================

// FROM (proto, constructed via construct_obj):
//   "save_trc"                                      (ro)
//     m_pod_instantiate: 42.0f
//     m_pod_share:       99.0f
//     m_obj_instantiate: → "trc_ref_instantiate#1"  (ro, name_of)
//     m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
//
// Save → VFS file → verify content → remove from cache → load from VFS
TEST_F(test_ctor, save_load_test_root_component)
{
    auto olc = make_olc();

    // --- construct a proto test_root_component ---
    core::object_constructor ctor(&olc);
    (void)ctor.create_default_class_obj_impl(get_rt("test_root_component"));

    root::test_root_component::construct_params params;
    auto result = ctor.construct_obj(AID("test_root_component"), AID("save_trc"), params, true);
    ASSERT_TRUE(result.has_value());
    auto* src = result.value()->as<root::test_root_component>();
    ASSERT_NE(src, nullptr);

    expect_obj(src, AID("save_trc"));
    expect_flags_proto(src);
    EXPECT_EQ(src->m_pod_instantiate, 42.0f);
    EXPECT_EQ(src->m_pod_share, 99.0f);
    expect_obj(src->m_obj_instantiate, AID("trc_ref_instantiate#1"));

    // --- save to VFS ---
    auto rc = ctor.save_obj(*src);
    ASSERT_EQ(rc, result_code::ok);

    // --- verify saved file content ---
    vfs::rid saved_rid;
    ASSERT_TRUE(olc.resolve(AID("save_trc"), saved_rid));

    auto& vfs = glob::glob_state().getr_vfs();
    std::string content;
    ASSERT_TRUE(vfs.read_string(saved_rid, content));
    EXPECT_EQ(content,
              "proto_id: test_root_component\n"
              "id: save_trc\n"
              "obj_instantiate: trc_ref_instantiate#1");

    // --- remove from cache, reload from VFS ---
    olc.remove_obj(*src);

    auto load_result = ctor.load_obj(AID("save_trc"));
    ASSERT_TRUE(load_result.has_value());
    auto* loaded = load_result.value()->as<root::test_root_component>();
    ASSERT_NE(loaded, nullptr);

    // --- verify roundtrip ---
    expect_obj(loaded, AID("save_trc"));
    expect_flags_proto(loaded);
    EXPECT_EQ(loaded->m_pod_instantiate, src->m_pod_instantiate);
    EXPECT_EQ(loaded->m_pod_share, src->m_pod_share);

    // --- cleanup saved file ---
    auto real = vfs.real_path(saved_rid);
    if (real)
    {
        std::filesystem::remove(*real);
    }
}

// construct_obj("test_root_object", "save_tro", params, is_proto=true)
//
// FROM (cdo):
//   "test_root_object"                              (cdo)
//     m_obj_instantiate: → "tro_ref_instantiate"    (ro, fixed)
//     m_obj_share:       → "tro_ref_share"          (ro, fixed)
//     components:
//       ├─ "root_component#1"                       (ro)
//       └─ "test_component#1" (test_root_component) (ro)
//
// TO (constructed proto):
//   "save_tro"                                      (ro)
//     m_obj_instantiate: → "tro_ref_instantiate#1"  (ro, name_of)
//     m_obj_share:       → "tro_ref_share"          (ro, shared from cdo)
//     components:
//       ├─ "root_component#2"                       (ro)
//       └─ "test_component#2" (test_root_component) (ro)
//
// Save diffs against CDO: obj_instantiate differs, obj_share same, pods same,
// components differ (different IDs).
TEST_F(test_ctor, save_load_test_root_object)
{
    auto olc = make_olc();
    core::object_constructor ctor(&olc);

    root::test_root_object::construct_params params;
    auto result = ctor.construct_obj(AID("test_root_object"), AID("save_tro"), params, true);
    ASSERT_TRUE(result.has_value());
    auto* src = result.value()->as<root::test_root_object>();
    ASSERT_NE(src, nullptr);

    expect_obj(src, AID("save_tro"));
    expect_flags_proto(src);

    // --- save to VFS ---
    auto rc = ctor.save_obj(*src);
    ASSERT_EQ(rc, result_code::ok);

    // --- verify saved file content ---
    vfs::rid saved_rid;
    ASSERT_TRUE(olc.resolve(AID("save_tro"), saved_rid));

    auto& vfs = glob::glob_state().getr_vfs();
    std::string content;
    ASSERT_TRUE(vfs.read_string(saved_rid, content));
    EXPECT_EQ(content,
              "proto_id: test_root_object\n"
              "id: save_tro\n"
              "obj_instantiate: tro_ref_instantiate#1\n"
              "layout: [-1, 0]\n"
              "components:\n"
              "  - id: root_component#2\n"
              "    proto_id: game_object_component\n"
              "  - id: test_component#2\n"
              "    proto_id: test_root_component\n"
              "    obj_instantiate: trc_ref_instantiate#2");

    // --- cleanup saved file ---
    auto real_p = vfs.real_path(saved_rid);
    if (real_p)
    {
        std::filesystem::remove(*real_p);
    }
}

// construct_obj("test_root_component", "save_inst_trc", params, is_proto=false)
//
// FROM (cdo):
//   "test_root_component"                             (cdo)
//     m_obj_instantiate: → "trc_ref_instantiate"      (ro, fixed)
//     m_obj_share:       → "trc_ref_share"            (ro, fixed)
//
// TO (constructed instance):
//   "save_inst_trc"                                   (inst)
//     m_obj_instantiate: → "trc_ref_instantiate#1"    (inst, name_of)
//     m_obj_share:       → "trc_ref_share"            (ro, shared from cdo)
TEST_F(test_ctor, save_instance_test_root_component)
{
    auto& olc = instance_olc();
    core::object_constructor ctor(&olc, core::object_load_type::instance_obj);
    ensure_package_cdo(AID("test_root_component"));

    root::test_root_component::construct_params params;
    auto result =
        ctor.construct_obj(AID("test_root_component"), AID("save_inst_trc"), params, false);
    ASSERT_TRUE(result.has_value());
    auto* src = result.value()->as<root::test_root_component>();
    ASSERT_NE(src, nullptr);

    expect_obj(src, AID("save_inst_trc"));
    expect_flags_instance(src);

    auto rc = ctor.save_obj(*src);
    ASSERT_EQ(rc, result_code::ok);

    vfs::rid saved_rid;
    ASSERT_TRUE(olc.resolve(AID("save_inst_trc"), saved_rid));

    auto& vfs = glob::glob_state().getr_vfs();
    std::string content;
    ASSERT_TRUE(vfs.read_string(saved_rid, content));
    EXPECT_EQ(content,
              "proto_id: test_root_component\n"
              "id: save_inst_trc\n"
              "obj_instantiate: trc_ref_instantiate#1");

    // --- remove from cache, reload as instance ---
    olc.remove_obj(*src);

    core::object_constructor ctor_inst(&olc, core::object_load_type::instance_obj);
    auto load_result = ctor_inst.load_obj(AID("save_inst_trc"));
    ASSERT_TRUE(load_result.has_value());
    auto* loaded = load_result.value()->as<root::test_root_component>();
    ASSERT_NE(loaded, nullptr);

    expect_obj(loaded, AID("save_inst_trc"));
    expect_flags_instance(loaded);
    EXPECT_EQ(loaded->m_pod_instantiate, src->m_pod_instantiate);
    EXPECT_EQ(loaded->m_pod_share, src->m_pod_share);

    auto real = vfs.real_path(saved_rid);
    if (real)
    {
        std::filesystem::remove(*real);
    }
}

// construct_obj("test_root_object", "save_inst_tro", params, is_proto=false)
//
// FROM (cdo):
//   "test_root_object"                              (cdo)
//     m_obj_instantiate: → "tro_ref_instantiate"    (ro, fixed)
//     m_obj_share:       → "tro_ref_share"          (ro, fixed)
//     components:
//       ├─ "root_component#1"                       (ro)
//       └─ "test_component#1" (test_root_component) (ro)
//
// TO (constructed instance):
//   "save_inst_tro"                                 (inst)
//     m_obj_instantiate: → "tro_ref_instantiate#1"  (inst, name_of)
//     m_obj_share:       → "tro_ref_share"          (ro, shared from cdo)
//     components:
//       ├─ "root_component#2"                       (inst)
//       └─ "test_component#2" (test_root_component) (inst)
TEST_F(test_ctor, save_instance_test_root_object)
{
    auto& olc = instance_olc();
    core::object_constructor ctor(&olc, core::object_load_type::instance_obj);
    ensure_package_cdo(AID("test_root_object"));

    root::test_root_object::construct_params params;
    auto result = ctor.construct_obj(AID("test_root_object"), AID("save_inst_tro"), params, false);
    ASSERT_TRUE(result.has_value());
    auto* src = result.value()->as<root::test_root_object>();
    ASSERT_NE(src, nullptr);

    expect_obj(src, AID("save_inst_tro"));
    expect_flags_instance(src);

    auto rc = ctor.save_obj(*src);
    ASSERT_EQ(rc, result_code::ok);

    vfs::rid saved_rid;
    ASSERT_TRUE(olc.resolve(AID("save_inst_tro"), saved_rid));

    auto& vfs = glob::glob_state().getr_vfs();
    std::string content;
    ASSERT_TRUE(vfs.read_string(saved_rid, content));
    EXPECT_EQ(content,
              "proto_id: test_root_object\n"
              "id: save_inst_tro\n"
              "obj_instantiate: tro_ref_instantiate#1\n"
              "layout: [-1, 0]\n"
              "components:\n"
              "  - id: root_component#2\n"
              "    proto_id: game_object_component\n"
              "  - id: test_component#2\n"
              "    proto_id: test_root_component\n"
              "    obj_instantiate: trc_ref_instantiate#2");

    auto real_p = vfs.real_path(saved_rid);
    if (real_p)
    {
        std::filesystem::remove(*real_p);
    }
}

// construct_obj("test_root_object", "layout_tro", params, is_proto=true)
// then add_test_component(test_component, "nested") + add_test_component(root, "sibling")
//
// FROM (cdo, created by preload_proto):
//   "test_root_object"                              (cdo)
//     m_obj_instantiate: → "tro_ref_instantiate"    (ro, fixed)
//     m_obj_share:       → "tro_ref_share"          (ro, fixed)
//     components:
//       ├─ "root_component#1"                       (ro, → cdo "game_object_component")
//       └─ "test_component#1" (test_root_component) (ro)
//            m_obj_instantiate: → "trc_ref_instantiate#1"  (ro, name_of)
//            m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
//
// TO (constructed proto, layout modified):
//   "layout_tro"                                    (ro)
//     m_obj_instantiate: → "tro_ref_instantiate#1"  (ro, name_of)
//     m_obj_share:       → "tro_ref_share"          (ro, shared from cdo)
//     components:
//       ├─ "root_component#2"                       (ro, → cdo "game_object_component")
//       │  ├─ "test_component#2" (test_root_component) (ro)
//       │  │    m_obj_instantiate: → "trc_ref_instantiate#2"  (ro, name_of)
//       │  │    m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
//       │  │  └─ "nested#1" (test_root_component)             (ro)
//       │  │       m_obj_instantiate: → "trc_ref_instantiate#3"  (ro, name_of)
//       │  │       m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
//       │  └─ "sibling#1" (test_root_component)               (ro)
//       │       m_obj_instantiate: → "trc_ref_instantiate#4"  (ro, name_of)
//       │       m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
//
// Layout (depth-first parent_idx): [-1, 0, 1, 0]
TEST_F(test_ctor, save_different_component_layout)
{
    auto olc = make_olc();
    core::object_constructor ctor(&olc);

    root::test_root_object::construct_params params;
    auto result = ctor.construct_obj(AID("test_root_object"), AID("layout_tro"), params, true);
    ASSERT_TRUE(result.has_value());
    auto* tro = result.value()->as<root::test_root_object>();
    expect_obj(tro, AID("layout_tro"));
    expect_flags_proto(tro);
    ASSERT_EQ(tro->get_subcomponents().size(), 2u);

    // --- add nested child under test_component ---
    auto* tc = tro->get_test_component();
    ASSERT_NE(tc, nullptr);
    auto* nested = tro->add_test_component(tc, AID("nested"));
    ASSERT_NE(nested, nullptr);

    // --- add sibling under root ---
    auto* sibling = tro->add_test_component(tro->get_root_component(), AID("sibling"));
    ASSERT_NE(sibling, nullptr);

    ASSERT_EQ(tro->get_subcomponents().size(), 4u);

    // --- verify identity + cache for all components ---
    expect_obj(tro->get_root_component(), AID("root_component#2"));
    expect_obj(tc, AID("test_component#2"));
    expect_obj(nested, AID("nested#1"));
    expect_obj(sibling, AID("sibling#1"));

    // --- verify flags on all components ---
    expect_flags_proto(tro->get_root_component());
    expect_flags_proto(tc);
    expect_flags_proto(nested);
    expect_flags_proto(sibling);

    // --- verify layout indices ---
    EXPECT_EQ(tro->get_root_component()->get_order_idx(), 0u);
    EXPECT_EQ(tro->get_root_component()->get_parent_idx(), root::NO_parent);
    EXPECT_EQ(tc->get_order_idx(), 1u);
    EXPECT_EQ(tc->get_parent_idx(), 0u);
    EXPECT_EQ(nested->get_order_idx(), 2u);
    EXPECT_EQ(nested->get_parent_idx(), 1u);
    EXPECT_EQ(sibling->get_order_idx(), 3u);
    EXPECT_EQ(sibling->get_parent_idx(), 0u);

    // --- verify component properties + refs ---
    EXPECT_EQ(tc->m_pod_instantiate, 42.0f);
    EXPECT_EQ(nested->m_pod_instantiate, 42.0f);
    EXPECT_EQ(sibling->m_pod_instantiate, 42.0f);
    expect_obj(tc->m_obj_instantiate, AID("trc_ref_instantiate#2"));
    expect_obj(nested->m_obj_instantiate, AID("trc_ref_instantiate#3"));
    expect_obj(sibling->m_obj_instantiate, AID("trc_ref_instantiate#4"));
    expect_flags_proto(tc->m_obj_instantiate);
    expect_flags_proto(nested->m_obj_instantiate);
    expect_flags_proto(sibling->m_obj_instantiate);

    // --- save ---
    auto rc = ctor.save_obj(*tro);
    ASSERT_EQ(rc, result_code::ok);

    // --- verify saved file content ---
    vfs::rid saved_rid;
    ASSERT_TRUE(olc.resolve(AID("layout_tro"), saved_rid));

    auto& vfs = glob::glob_state().getr_vfs();
    std::string content;
    ASSERT_TRUE(vfs.read_string(saved_rid, content));
    EXPECT_EQ(content,
              "proto_id: test_root_object\n"
              "id: layout_tro\n"
              "obj_instantiate: tro_ref_instantiate#1\n"
              "layout: [-1, 0, 1, 0]\n"
              "components:\n"
              "  - id: root_component#2\n"
              "    proto_id: game_object_component\n"
              "  - id: test_component#2\n"
              "    proto_id: test_root_component\n"
              "    obj_instantiate: trc_ref_instantiate#2\n"
              "  - id: nested#1\n"
              "    proto_id: test_root_component\n"
              "    obj_instantiate: trc_ref_instantiate#3\n"
              "  - id: sibling#1\n"
              "    proto_id: test_root_component\n"
              "    obj_instantiate: trc_ref_instantiate#4");

    // --- cleanup ---
    auto real_p = vfs.real_path(saved_rid);
    if (real_p)
    {
        std::filesystem::remove(*real_p);
    }
}

// construct_obj("test_root_object", "layout_inst_tro", params, is_proto=false)
// then add_test_component(test_component, "nested") + add_test_component(root, "sibling")
//
// FROM (cdo, created by preload_proto):
//   "test_root_object"                              (cdo)
//     m_obj_instantiate: → "tro_ref_instantiate"    (ro, fixed)
//     m_obj_share:       → "tro_ref_share"          (ro, fixed)
//     components:
//       ├─ "root_component#1"                       (ro, → cdo "game_object_component")
//       └─ "test_component#1" (test_root_component) (ro)
//            m_obj_instantiate: → "trc_ref_instantiate#1"  (ro, name_of)
//            m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
//
// TO (constructed instance, layout modified):
//   "layout_inst_tro"                               (inst)
//     m_obj_instantiate: → "tro_ref_instantiate#1"  (inst, name_of)
//     m_obj_share:       → "tro_ref_share"          (ro, shared from cdo)
//     components:
//       ├─ "root_component#2"                       (inst, → cdo "game_object_component")
//       │  ├─ "test_component#2" (test_root_component) (inst)
//       │  │    m_obj_instantiate: → "trc_ref_instantiate#2"  (inst, name_of)
//       │  │    m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
//       │  │  └─ "nested#1" (test_root_component)             (inst)
//       │  │       m_obj_instantiate: → "trc_ref_instantiate#3"  (inst, name_of)
//       │  │       m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
//       │  └─ "sibling#1" (test_root_component)               (inst)
//       │       m_obj_instantiate: → "trc_ref_instantiate#4"  (inst, name_of)
//       │       m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
//
// Layout (depth-first parent_idx): [-1, 0, 1, 0]
TEST_F(test_ctor, save_instance_different_component_layout)
{
    auto& olc = instance_olc();
    core::object_constructor ctor(&olc, core::object_load_type::instance_obj);
    ensure_package_cdo(AID("test_root_object"));

    root::test_root_object::construct_params params;
    auto result =
        ctor.construct_obj(AID("test_root_object"), AID("layout_inst_tro"), params, false);
    ASSERT_TRUE(result.has_value());
    auto* tro = result.value()->as<root::test_root_object>();
    expect_obj(tro, AID("layout_inst_tro"));
    expect_flags_instance(tro);
    ASSERT_EQ(tro->get_subcomponents().size(), 2u);

    // --- add nested child under test_component ---
    auto* tc = tro->get_test_component();
    ASSERT_NE(tc, nullptr);
    auto* nested = tro->add_test_component(tc, AID("nested"));
    ASSERT_NE(nested, nullptr);

    // --- add sibling under root ---
    auto* sibling = tro->add_test_component(tro->get_root_component(), AID("sibling"));
    ASSERT_NE(sibling, nullptr);

    ASSERT_EQ(tro->get_subcomponents().size(), 4u);

    // --- verify identity + cache for all components ---
    expect_obj(tro->get_root_component(), AID("root_component#2"));
    expect_obj(tc, AID("test_component#2"));
    expect_obj(nested, AID("nested#1"));
    expect_obj(sibling, AID("sibling#1"));

    // --- verify instance flags on all ---
    expect_flags_instance(tro->get_root_component());
    expect_flags_instance(tc);
    expect_flags_instance(nested);
    expect_flags_instance(sibling);

    // --- verify layout indices ---
    EXPECT_EQ(tro->get_root_component()->get_order_idx(), 0u);
    EXPECT_EQ(tro->get_root_component()->get_parent_idx(), root::NO_parent);
    EXPECT_EQ(tc->get_order_idx(), 1u);
    EXPECT_EQ(tc->get_parent_idx(), 0u);
    EXPECT_EQ(nested->get_order_idx(), 2u);
    EXPECT_EQ(nested->get_parent_idx(), 1u);
    EXPECT_EQ(sibling->get_order_idx(), 3u);
    EXPECT_EQ(sibling->get_parent_idx(), 0u);

    // --- verify component properties + refs ---
    EXPECT_EQ(tc->m_pod_instantiate, 42.0f);
    EXPECT_EQ(nested->m_pod_instantiate, 42.0f);
    EXPECT_EQ(sibling->m_pod_instantiate, 42.0f);
    expect_obj(tc->m_obj_instantiate, AID("trc_ref_instantiate#2"));
    expect_obj(nested->m_obj_instantiate, AID("trc_ref_instantiate#3"));
    expect_obj(sibling->m_obj_instantiate, AID("trc_ref_instantiate#4"));
    expect_flags_instance(tc->m_obj_instantiate);
    expect_flags_instance(nested->m_obj_instantiate);
    expect_flags_instance(sibling->m_obj_instantiate);

    // --- save ---
    auto rc = ctor.save_obj(*tro);
    ASSERT_EQ(rc, result_code::ok);

    // --- verify saved file content ---
    vfs::rid saved_rid;
    ASSERT_TRUE(olc.resolve(AID("layout_inst_tro"), saved_rid));

    auto& vfs = glob::glob_state().getr_vfs();
    std::string content;
    ASSERT_TRUE(vfs.read_string(saved_rid, content));
    EXPECT_EQ(content,
              "proto_id: test_root_object\n"
              "id: layout_inst_tro\n"
              "obj_instantiate: tro_ref_instantiate#1\n"
              "layout: [-1, 0, 1, 0]\n"
              "components:\n"
              "  - id: root_component#2\n"
              "    proto_id: game_object_component\n"
              "  - id: test_component#2\n"
              "    proto_id: test_root_component\n"
              "    obj_instantiate: trc_ref_instantiate#2\n"
              "  - id: nested#1\n"
              "    proto_id: test_root_component\n"
              "    obj_instantiate: trc_ref_instantiate#3\n"
              "  - id: sibling#1\n"
              "    proto_id: test_root_component\n"
              "    obj_instantiate: trc_ref_instantiate#4");

    // --- cleanup ---
    auto real_p = vfs.real_path(saved_rid);
    if (real_p)
    {
        std::filesystem::remove(*real_p);
    }
}

// ============================================================================
// negative: malformed component YAML
// ============================================================================

// Component proto_id references an instance object, not a CDO type.
// The handler should reject it — deriving from an instance is invalid.
TEST_F(test_ctor, load_component_type_is_instance)
{
    auto& olc = instance_olc();
    core::object_constructor ctor(&olc, core::object_load_type::instance_obj);

    // Create CDOs + an instance of test_root_component
    ensure_package_cdo(AID("test_root_object"));
    root::test_root_component::construct_params cp;
    auto inst = ctor.construct_obj(AID("test_root_component"), AID("inst_trc"), cp, false);
    ASSERT_TRUE(inst.has_value());
    expect_flags_instance(inst.value());

    auto vfs_root = olc.get_vfs_root();
    auto& vfs = glob::glob_state().getr_vfs();

    std::string relative = "class/bad_inst_tro.aobj";
    auto rid = vfs_root / relative;
    ASSERT_TRUE(vfs.write_string(rid,
                                 "proto_id: test_root_object\n"
                                 "id: bad_inst_tro\n"
                                 "layout: [-1, 0]\n"
                                 "components:\n"
                                 "  - id: ok_root#1\n"
                                 "    proto_id: game_object_component\n"
                                 "  - id: bad_comp#1\n"
                                 "    proto_id: inst_trc"));
    vfs.register_object(vfs_root, "bad_inst_tro", relative);

    auto result = ctor.load_obj(AID("bad_inst_tro"));
    EXPECT_FALSE(result.has_value());

    auto real_p = vfs.real_path(rid);
    if (real_p)
    {
        std::filesystem::remove(*real_p);
    }
}

// Component proto_id references a type that doesn't exist at all.
TEST_F(test_ctor, load_component_type_nonexistent)
{
    auto olc = make_olc();
    core::object_constructor ctor(&olc);
    (void)ctor.create_default_class_obj_impl(get_rt("test_root_object"));

    auto vfs_root = olc.get_vfs_root();
    auto& vfs = glob::glob_state().getr_vfs();

    std::string relative = "class/bad_type_tro.aobj";
    auto rid = vfs_root / relative;
    ASSERT_TRUE(vfs.write_string(rid,
                                 "proto_id: test_root_object\n"
                                 "id: bad_type_tro\n"
                                 "layout: [-1, 0]\n"
                                 "components:\n"
                                 "  - id: ok_root#1\n"
                                 "    proto_id: game_object_component\n"
                                 "  - id: bad_comp#1\n"
                                 "    proto_id: nonexistent_type"));
    vfs.register_object(vfs_root, "bad_type_tro", relative);

    auto result = ctor.load_obj(AID("bad_type_tro"));
    EXPECT_FALSE(result.has_value());

    auto real_p = vfs.real_path(rid);
    if (real_p)
    {
        std::filesystem::remove(*real_p);
    }
}

// Two components share the same id — second one should be rejected.
TEST_F(test_ctor, load_component_duplicate_ids)
{
    auto olc = make_olc();
    core::object_constructor ctor(&olc);
    (void)ctor.create_default_class_obj_impl(get_rt("test_root_object"));

    auto vfs_root = olc.get_vfs_root();
    auto& vfs = glob::glob_state().getr_vfs();

    std::string relative = "class/dup_ids_tro.aobj";
    auto rid = vfs_root / relative;
    ASSERT_TRUE(vfs.write_string(rid,
                                 "proto_id: test_root_object\n"
                                 "id: dup_ids_tro\n"
                                 "layout: [-1, 0]\n"
                                 "components:\n"
                                 "  - id: dup_comp\n"
                                 "    proto_id: game_object_component\n"
                                 "  - id: dup_comp\n"
                                 "    proto_id: test_root_component"));
    vfs.register_object(vfs_root, "dup_ids_tro", relative);

    auto result = ctor.load_obj(AID("dup_ids_tro"));
    EXPECT_FALSE(result.has_value());

    auto real_p = vfs.real_path(rid);
    if (real_p)
    {
        std::filesystem::remove(*real_p);
    }
}

// A component loaded from a standalone file must not also appear
// as an inline component inside a game_object's layout.
TEST_F(test_ctor, load_component_conflicts_with_standalone_file)
{
    auto olc = make_olc();
    core::object_constructor ctor(&olc);
    (void)ctor.create_default_class_obj_impl(get_rt("test_root_object"));

    auto vfs_root = olc.get_vfs_root();
    auto& vfs = glob::glob_state().getr_vfs();

    // --- write + load a standalone component file ---
    std::string comp_relative = "class/standalone_comp.aobj";
    auto comp_rid = vfs_root / comp_relative;
    ASSERT_TRUE(vfs.write_string(comp_rid,
                                 "proto_id: test_root_component\n"
                                 "id: standalone_comp"));
    vfs.register_object(vfs_root, "standalone_comp", comp_relative);

    auto comp_result = ctor.load_obj(AID("standalone_comp"));
    ASSERT_TRUE(comp_result.has_value());
    expect_obj(comp_result.value(), AID("standalone_comp"));

    // --- write a game_object that reuses the same id inline ---
    std::string go_relative = "class/conflict_tro.aobj";
    auto go_rid = vfs_root / go_relative;
    ASSERT_TRUE(vfs.write_string(go_rid,
                                 "proto_id: test_root_object\n"
                                 "id: conflict_tro\n"
                                 "layout: [-1, 0]\n"
                                 "components:\n"
                                 "  - id: ok_root#1\n"
                                 "    proto_id: game_object_component\n"
                                 "  - id: standalone_comp\n"
                                 "    proto_id: test_root_component"));
    vfs.register_object(vfs_root, "conflict_tro", go_relative);

    // --- load should fail: standalone_comp already in cache ---
    auto go_result = ctor.load_obj(AID("conflict_tro"));
    EXPECT_FALSE(go_result.has_value());

    // --- cleanup ---
    auto rp1 = vfs.real_path(comp_rid);
    if (rp1)
    {
        std::filesystem::remove(*rp1);
    }
    auto rp2 = vfs.real_path(go_rid);
    if (rp2)
    {
        std::filesystem::remove(*rp2);
    }
}
