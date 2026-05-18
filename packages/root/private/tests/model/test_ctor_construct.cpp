#include "test_ctor_fixture.h"

// Expected object layout after create_default_class_obj_impl("test_root_object"):
//
//   "test_root_object"                              (cdo)
//     m_pod_instantiate: 42.0f
//     m_pod_share:       99.0f
//     m_obj_instantiate: → "tro_ref_instantiate"    (ro, fixed)
//     m_obj_share:       → "tro_ref_share"          (ro, fixed)
//     components:
//       ├─ "root_component#1"                       (ro, → cdo "game_object_component")
//       └─ "test_component#1" (test_root_component) (ro, → cdo "test_root_component")
//            m_obj_instantiate: → "trc_ref_instantiate#1"  (ro, name_of)
//            m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
TEST_F(test_ctor, create_default_test_root_object)
{
    auto olc = make_olc();
    core::object_constructor ctor(&olc);

    auto result = ctor.create_default_class_obj_impl(get_rt("test_root_object"));
    ASSERT_TRUE(result.has_value());

    auto* tro = result.value()->as<root::test_root_object>();
    ASSERT_NE(tro, nullptr);

    auto& global = glob::glob_state().getr_model().caches;

    // --- test_root_object CDO ---
    expect_obj(tro, AID("test_root_object"));
    expect_flags_cdo(tro);
    EXPECT_EQ(tro->get_state(), root::smart_object_state::constructed);
    EXPECT_EQ(tro->m_pod_instantiate, 42.0f);
    EXPECT_EQ(tro->m_pod_share, 99.0f);
    EXPECT_EQ(olc.find_obj(AID("test_root_object")), tro);

    // --- test_root_object smart_object refs ---
    expect_obj(tro->m_obj_instantiate, AID("tro_ref_instantiate"));
    expect_obj(tro->m_obj_share, AID("tro_ref_share"));
    EXPECT_NE(tro->m_obj_instantiate, tro->m_obj_share);
    expect_flags_proto(tro->m_obj_instantiate);
    expect_flags_proto(tro->m_obj_share);

    // --- components: 2 total ---
    ASSERT_EQ(tro->get_subcomponents().size(), 2u);

    // --- root_component ---
    auto* root_comp = tro->get_root_component();
    expect_obj(root_comp, AID("root_component#1"));
    expect_flags_proto(root_comp);
    EXPECT_EQ(global.components.get_item(AID("root_component#1")), root_comp);

    // --- root_component's CDO (game_object_component) ---
    auto* goc_cdo = const_cast<root::smart_object*>(root_comp->get_class_obj());
    expect_obj(goc_cdo, AID("game_object_component"));
    expect_flags_cdo(goc_cdo);

    // --- test_root_component ---
    auto* tc = tro->get_test_component();
    expect_obj(tc, AID("test_component#1"));
    expect_flags_proto(tc);
    EXPECT_EQ(tc->m_pod_instantiate, 42.0f);
    EXPECT_EQ(tc->m_pod_share, 99.0f);
    EXPECT_EQ(global.components.get_item(AID("test_component#1")), tc);

    // --- test_root_component's CDO ---
    auto* trc_cdo = const_cast<root::smart_object*>(tc->get_class_obj());
    expect_obj(trc_cdo, AID("test_root_component"));
    expect_flags_cdo(trc_cdo);

    // --- test_root_component smart_object refs ---
    expect_obj(tc->m_obj_instantiate, AID("trc_ref_instantiate#1"));
    expect_flags_proto(tc->m_obj_instantiate);
    EXPECT_EQ(tc->m_obj_share->get_id(), AID("trc_ref_share"));
    EXPECT_EQ(tc->m_obj_share, trc_cdo->as<root::test_root_component>()->m_obj_share);

    // --- smart_object CDO ---
    auto* so_cdo = global.objects.get_item(AID("smart_object"));
    ASSERT_NE(so_cdo, nullptr);
    expect_flags_cdo(so_cdo);
}

// Expected object layout after create_default_class_obj_impl("test_root_component"):
//
//   "test_root_component"                             (cdo)
//     m_pod_instantiate: 42.0f
//     m_pod_share:       99.0f
//     m_obj_instantiate: → "trc_ref_instantiate"      (ro, fixed)
//     m_obj_share:       → "trc_ref_share"            (ro, fixed)
//
//   Objects in cache (4):
//     "test_root_component"        — CDO
//     "smart_object"               — CDO, preload_proto for smart_objects
//     "trc_ref_instantiate"        — CDO smart_object (fixed name)
//     "trc_ref_share"              — CDO smart_object (fixed name)
TEST_F(test_ctor, create_default_test_root_component)
{
    auto olc = make_olc();
    core::object_constructor ctor(&olc);

    auto result = ctor.create_default_class_obj_impl(get_rt("test_root_component"));
    ASSERT_TRUE(result.has_value());

    auto* obj = result.value();
    auto* tc = obj->as<root::test_root_component>();
    ASSERT_NE(tc, nullptr);

    auto& global = glob::glob_state().getr_model().caches;

    // --- test_root_component CDO ---
    expect_obj(obj, AID("test_root_component"));
    expect_flags_cdo(obj);
    EXPECT_EQ(obj->get_architype_id(), core::architype::component);
    EXPECT_EQ(obj->get_package(), &root::package::instance());
    EXPECT_EQ(obj->get_state(), root::smart_object_state::constructed);
    EXPECT_EQ(tc->m_pod_instantiate, 42.0f);
    EXPECT_EQ(tc->m_pod_share, 99.0f);
    EXPECT_EQ(global.components.get_item(AID("test_root_component")), obj);
    EXPECT_EQ(m_local_cs.components.get_item(AID("test_root_component")), obj);
    EXPECT_EQ(olc.find_obj(AID("test_root_component")), obj);

    // --- smart_object refs ---
    expect_obj(tc->m_obj_instantiate, AID("trc_ref_instantiate"));
    expect_obj(tc->m_obj_share, AID("trc_ref_share"));
    EXPECT_NE(tc->m_obj_instantiate, tc->m_obj_share);
    expect_flags_proto(tc->m_obj_instantiate);
    expect_flags_proto(tc->m_obj_share);

    // --- smart_object CDO ---
    auto* so_cdo = global.objects.get_item(AID("smart_object"));
    ASSERT_NE(so_cdo, nullptr);
    expect_flags_cdo(so_cdo);
}

TEST_F(test_ctor, find_obj_returns_null_for_missing)
{
    auto olc = make_olc();
    EXPECT_EQ(olc.find_obj(AID("nonexistent")), nullptr);
    EXPECT_EQ(olc.find_obj(AID("nonexistent"), core::architype::game_object), nullptr);
}

// ============================================================================
// construct_obj with is_proto flag
// ============================================================================

// construct_obj("test_root_object", "ctor_proto_tro", params, is_proto=true)
//
// FROM (cdo, created by preload_proto):
//   "test_root_object"                              (cdo)
//     m_obj_instantiate: → "tro_ref_instantiate"    (ro, fixed)
//     m_obj_share:       → "tro_ref_share"          (ro, fixed)
//     components:
//       └─ "test_component#1" (test_root_component) (ro)
//            m_obj_instantiate: → "trc_ref_instantiate#1"  (ro, name_of)
//            m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
//
// TO (constructed proto):
//   "ctor_proto_tro"                                (ro)
//     m_obj_instantiate: → "tro_ref_instantiate#1"  (ro, name_of)
//     m_obj_share:       → "tro_ref_share"          (ro, shared from cdo)
//     components:
//       └─ "test_component#2" (test_root_component) (ro)
//            m_obj_instantiate: → "trc_ref_instantiate#2"  (ro, name_of)
//            m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
TEST_F(test_ctor, construct_obj_as_proto)
{
    auto olc = make_olc();

    root::test_root_object::construct_params params;

    auto result = core::object_constructor(&olc).construct_obj(
        AID("test_root_object"), AID("ctor_proto_tro"), params, true);

    ASSERT_TRUE(result.has_value());
    auto* obj = result.value();
    auto* tro = obj->as<root::test_root_object>();
    ASSERT_TRUE(tro);

    expect_obj(obj, AID("ctor_proto_tro"));
    EXPECT_EQ(obj->get_type_id(), AID("test_root_object"));
    expect_flags_proto(obj);
    EXPECT_EQ(tro->m_pod_instantiate, 42.0f);
    EXPECT_EQ(tro->m_pod_share, 99.0f);

    // --- tro smart_object refs ---
    expect_obj(tro->m_obj_instantiate, AID("tro_ref_instantiate#1"));
    expect_flags_proto(tro->m_obj_instantiate);
    EXPECT_EQ(tro->m_obj_share->get_id(), AID("tro_ref_share"));

    // --- class_obj → cdo ---
    auto* cdo = const_cast<root::smart_object*>(obj->get_class_obj());
    ASSERT_NE(cdo, nullptr);
    expect_flags_cdo(cdo);
    EXPECT_EQ(cdo->get_id(), AID("test_root_object"));
    EXPECT_EQ(tro->m_obj_share, cdo->as<root::test_root_object>()->m_obj_share);

    // --- components ---
    ASSERT_EQ(tro->get_subcomponents().size(), 2u);

    auto* root_comp = tro->get_root_component();
    expect_obj(root_comp, AID("root_component#2"));
    expect_flags_proto(root_comp);
    auto* rc_cdo = root_comp->get_class_obj();
    ASSERT_NE(rc_cdo, nullptr);
    expect_flags_cdo(const_cast<root::smart_object*>(rc_cdo));

    auto* tc = tro->get_test_component();
    expect_obj(tc, AID("test_component#2"));
    expect_flags_proto(tc);
    EXPECT_EQ(tc->m_pod_instantiate, 42.0f);
    EXPECT_EQ(tc->m_pod_share, 99.0f);

    // --- tc class_obj → cdo ---
    auto* trc_cdo = const_cast<root::smart_object*>(tc->get_class_obj());
    ASSERT_NE(trc_cdo, nullptr);
    expect_flags_cdo(trc_cdo);

    // --- tc smart_object refs ---
    expect_obj(tc->m_obj_instantiate, AID("trc_ref_instantiate#2"));
    expect_flags_proto(tc->m_obj_instantiate);
    EXPECT_EQ(tc->m_obj_share->get_id(), AID("trc_ref_share"));
    EXPECT_EQ(tc->m_obj_share, trc_cdo->as<root::test_root_component>()->m_obj_share);
}

// construct_obj("test_root_object", "ctor_inst_tro", params, is_proto=false)
//
// FROM (cdo, created by preload_proto):
//   "test_root_object"                              (cdo)
//     m_obj_instantiate: → "tro_ref_instantiate"    (ro, fixed)
//     m_obj_share:       → "tro_ref_share"          (ro, fixed)
//     components:
//       └─ "test_component#1" (test_root_component) (ro)
//            m_obj_instantiate: → "trc_ref_instantiate#1"  (ro, name_of)
//            m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
//
// TO (constructed instance):
//   "ctor_inst_tro"                                 (inst)
//     m_obj_instantiate: → "tro_ref_instantiate#1"  (inst, name_of)
//     m_obj_share:       → "tro_ref_share"          (ro, shared from cdo)
//     components:
//       └─ "test_component#2" (test_root_component) (inst)
//            m_obj_instantiate: → "trc_ref_instantiate#2"  (inst, name_of)
//            m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
TEST_F(test_ctor, construct_obj_as_instance)
{
    auto olc = make_olc();

    root::test_root_object::construct_params params;
    params.pos = {5.0f, 6.0f, 7.0f};

    auto result = core::object_constructor(&olc).construct_obj(
        AID("test_root_object"), AID("ctor_inst_tro"), params, false);

    ASSERT_TRUE(result.has_value());
    auto* obj = result.value();
    auto* tro = obj->as<root::test_root_object>();
    ASSERT_TRUE(tro);

    expect_obj(obj, AID("ctor_inst_tro"));
    expect_flags_instance(obj);
    EXPECT_EQ(tro->get_position(), root::vec3(5.0f, 6.0f, 7.0f));
    EXPECT_EQ(tro->m_pod_instantiate, 42.0f);
    EXPECT_EQ(tro->m_pod_share, 99.0f);

    // --- tro smart_object refs ---
    expect_obj(tro->m_obj_instantiate, AID("tro_ref_instantiate#1"));
    expect_flags_instance(tro->m_obj_instantiate);
    EXPECT_EQ(tro->m_obj_share->get_id(), AID("tro_ref_share"));

    // --- tro share points to CDO's object ---
    auto* cdo = const_cast<root::smart_object*>(obj->get_class_obj());
    EXPECT_EQ(tro->m_obj_share, cdo->as<root::test_root_object>()->m_obj_share);

    // --- components ---
    ASSERT_EQ(tro->get_subcomponents().size(), 2u);

    auto* root_comp = tro->get_root_component();
    expect_obj(root_comp, AID("root_component#2"));
    expect_flags_instance(root_comp);

    auto* tc = tro->get_test_component();
    expect_obj(tc, AID("test_component#2"));
    expect_flags_instance(tc);
    EXPECT_EQ(tc->m_pod_instantiate, 42.0f);
    EXPECT_EQ(tc->m_pod_share, 99.0f);

    // --- tc smart_object refs ---
    expect_obj(tc->m_obj_instantiate, AID("trc_ref_instantiate#2"));
    expect_flags_instance(tc->m_obj_instantiate);
    EXPECT_EQ(tc->m_obj_share->get_id(), AID("trc_ref_share"));

    // --- tc share points to CDO's object ---
    auto* trc_cdo = const_cast<root::smart_object*>(tc->get_class_obj());
    EXPECT_EQ(tc->m_obj_share, trc_cdo->as<root::test_root_component>()->m_obj_share);
}

TEST_F(test_ctor, construct_obj_invalid_type)
{
    auto olc = make_olc();

    root::smart_object::construct_params params;
    auto result = core::object_constructor(&olc).construct_obj(
        AID("nonexistent_type_xyz"), AID("should_fail"), params, true);

    ASSERT_FALSE(result.has_value());

    auto& global = glob::glob_state().getr_model().caches;
    EXPECT_EQ(global.objects.get_item(AID("should_fail")), nullptr)
        << "failed construct should not leave objects in global cache";
    EXPECT_EQ(olc.find_obj(AID("should_fail")), nullptr)
        << "failed construct should not leave objects in cache";
}

TEST_F(test_ctor, construct_obj_proto_over_instance_fails)
{
    auto olc = make_olc();
    root::test_root_object::construct_params params;

    auto inst = core::object_constructor(&olc).construct_obj(
        AID("test_root_object"), AID("conflict_id"), params, false);
    ASSERT_TRUE(inst.has_value());

    auto proto = core::object_constructor(&olc).construct_obj(
        AID("test_root_object"), AID("conflict_id"), params, true);
    EXPECT_FALSE(proto.has_value());
}

TEST_F(test_ctor, construct_obj_instance_over_proto_fails)
{
    auto olc = make_olc();
    root::test_root_object::construct_params params;

    auto proto = core::object_constructor(&olc).construct_obj(
        AID("test_root_object"), AID("conflict_id"), params, true);
    ASSERT_TRUE(proto.has_value());

    auto inst = core::object_constructor(&olc).construct_obj(
        AID("test_root_object"), AID("conflict_id"), params, false);
    EXPECT_FALSE(inst.has_value());
}
