#include "test_ctor_fixture.h"

// ============================================================================
// instantiate_obj
// ============================================================================

// instantiate_obj from a proto constructed with is_proto=true
//
// FROM (proto, constructed via construct_obj):
//   "inst_proto"                                    (ro)
//     m_obj_instantiate: → "tro_ref_instantiate#1"  (ro, name_of)
//     m_obj_share:       → "tro_ref_share"          (ro, shared from cdo)
//     components:
//       ├─ "root_component#2"                       (ro)
//       └─ "test_component#2" (test_root_component) (ro)
//            m_obj_instantiate: → "trc_ref_instantiate#2"  (ro, name_of)
//            m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
//
// TO (instantiated instance, class_obj → proto):
//   "inst_from_proto"                               (inst, class_obj → "inst_proto")
//     m_obj_instantiate: → "tro_ref_instantiate#2"  (inst, new instance)
//     m_obj_share:       → "tro_ref_share"          (ro, memcpy)
//     components:
//       ├─ "root_component#3"                       (inst)
//       └─ "test_component#3" (test_root_component) (inst)
//            m_obj_instantiate: → "trc_ref_instantiate#3"  (inst, new instance)
//            m_obj_share:       → "trc_ref_share"          (ro, memcpy)
TEST_F(test_ctor, instantiate_obj_from_proto)
{
    auto olc = make_olc();

    root::test_root_object::construct_params params;
    auto proto_result = core::object_constructor(&olc).construct_obj(
        AID("test_root_object"), AID("inst_proto"), params, true);
    ASSERT_TRUE(proto_result.has_value());
    auto* proto = proto_result.value();
    auto* proto_tro = proto->as<root::test_root_object>();

    core::object_constructor ctor(&olc);
    auto inst_result = ctor.instantiate_obj(*proto, AID("inst_from_proto"));
    ASSERT_TRUE(inst_result.has_value());
    auto* inst = inst_result.value();
    auto* inst_tro = inst->as<root::test_root_object>();
    ASSERT_TRUE(inst_tro);

    // --- instance identity ---
    expect_obj(inst, AID("inst_from_proto"));
    expect_flags_instance(inst);
    EXPECT_NE(inst, proto);
    EXPECT_EQ(inst->get_class_obj(), proto);

    // --- PODs copied ---
    EXPECT_EQ(inst_tro->m_pod_instantiate, 42.0f);
    EXPECT_EQ(inst_tro->m_pod_share, 99.0f);

    // --- m_obj_instantiate: new instance (instantiate mode always creates new) ---
    expect_obj(inst_tro->m_obj_instantiate, AID("tro_ref_instantiate#2"));
    expect_flags_instance(inst_tro->m_obj_instantiate);
    EXPECT_NE(inst_tro->m_obj_instantiate, proto_tro->m_obj_instantiate);

    // --- m_obj_share: shared from proto (share mode = memcpy pointer) ---
    EXPECT_EQ(inst_tro->m_obj_share, proto_tro->m_obj_share);

    // --- components: new objects, same count ---
    ASSERT_EQ(inst_tro->get_subcomponents().size(), 2u);

    auto* inst_root = inst_tro->get_root_component();
    expect_obj(inst_root, AID("root_component#3"));
    expect_flags_instance(inst_root);
    EXPECT_NE(inst_root, proto_tro->get_root_component());

    auto* inst_tc = inst_tro->get_test_component();
    expect_obj(inst_tc, AID("test_component#3"));
    expect_flags_instance(inst_tc);
    EXPECT_NE(inst_tc, proto_tro->get_test_component());
    EXPECT_EQ(inst_tc->get_class_obj(), proto_tro->get_test_component());

    // --- tc properties ---
    auto* proto_tc = proto_tro->get_test_component();
    EXPECT_EQ(inst_tc->m_pod_instantiate, 42.0f);
    EXPECT_EQ(inst_tc->m_pod_share, 99.0f);

    // --- tc m_obj_instantiate: new instance ---
    expect_obj(inst_tc->m_obj_instantiate, AID("trc_ref_instantiate#3"));
    expect_flags_instance(inst_tc->m_obj_instantiate);
    EXPECT_NE(inst_tc->m_obj_instantiate, proto_tc->m_obj_instantiate);

    // --- tc m_obj_share: shared from proto's component ---
    EXPECT_EQ(inst_tc->m_obj_share, proto_tc->m_obj_share);
}

TEST_F(test_ctor, instantiate_obj_from_instance_fails)
{
    auto olc = make_olc();

    root::test_root_object::construct_params params;
    auto proto_result = core::object_constructor(&olc).construct_obj(
        AID("test_root_object"), AID("inst_assert_proto"), params, true);
    ASSERT_TRUE(proto_result.has_value());

    core::object_constructor ctor1(&olc);
    auto inst_result = ctor1.instantiate_obj(*proto_result.value(), AID("inst_assert_inst"));
    ASSERT_TRUE(inst_result.has_value());
    auto* instance = inst_result.value();
    ASSERT_TRUE(instance->get_flags().instance_obj);

    core::object_constructor ctor2(&olc);
    auto result = ctor2.instantiate_obj(*instance, AID("inst_from_inst"));
    EXPECT_FALSE(result.has_value());
}
