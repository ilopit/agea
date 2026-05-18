#include "test_ctor_fixture.h"

// ============================================================================
// clone_obj
// ============================================================================

// clone_obj from a proto with m_mode=class_obj (proto clone)
//
// FROM (proto, constructed via construct_obj):
//   "clone_proto_src"                               (ro)
//     m_obj_instantiate: → "tro_ref_instantiate#1"  (ro, name_of)
//     m_obj_share:       → "tro_ref_share"          (ro, shared from cdo)
//     components:
//       ├─ "root_component#2"                       (ro)
//       └─ "test_component#2" (test_root_component) (ro)
//            m_obj_instantiate: → "trc_ref_instantiate#2"  (ro, name_of)
//            m_obj_share:       → "trc_ref_share"          (ro, shared from cdo)
//
// TO (proto clone, class_obj → src):
//   "clone_proto_dst"                               (ro, class_obj → "clone_proto_src")
//     m_obj_instantiate: → "tro_ref_instantiate#2"  (ro, cloned from #1)
//     m_obj_share:       → "tro_ref_share"          (ro, shared from src)
//     components:
//       ├─ "root_component#3"                       (ro, cloned from #2)
//       └─ "test_component#3" (test_root_component) (ro, cloned from #2)
//            m_obj_instantiate: → "trc_ref_instantiate#3"  (ro, cloned from #2)
//            m_obj_share:       → "trc_ref_share"          (ro, shared from src)
TEST_F(test_ctor, clone_obj_as_proto)
{
    auto olc = make_olc();

    root::test_root_object::construct_params params;
    auto src_result = core::object_constructor(&olc).construct_obj(
        AID("test_root_object"), AID("clone_proto_src"), params, true);
    ASSERT_TRUE(src_result.has_value());
    auto* src = src_result.value();
    auto* src_tro = src->as<root::test_root_object>();

    core::object_constructor ctor(&olc);
    auto clone_result = ctor.clone_obj(*src, AID("clone_proto_dst"));
    ASSERT_TRUE(clone_result.has_value());
    auto* clone = clone_result.value();
    auto* clone_tro = clone->as<root::test_root_object>();
    ASSERT_TRUE(clone_tro);

    // --- clone identity ---
    expect_obj(clone, AID("clone_proto_dst"));
    expect_flags_proto(clone);
    EXPECT_NE(clone, src);
    EXPECT_EQ(clone->get_class_obj(), src);

    // --- PODs copied ---
    EXPECT_EQ(clone_tro->m_pod_instantiate, 42.0f);
    EXPECT_EQ(clone_tro->m_pod_share, 99.0f);

    // --- m_obj_instantiate: new proto (clone_obj respects m_mode) ---
    expect_obj(clone_tro->m_obj_instantiate, AID("tro_ref_instantiate#2"));
    expect_flags_proto(clone_tro->m_obj_instantiate);
    EXPECT_NE(clone_tro->m_obj_instantiate, src_tro->m_obj_instantiate);

    // --- m_obj_share: shared from src ---
    EXPECT_EQ(clone_tro->m_obj_share, src_tro->m_obj_share);

    // --- components: cloned with new IDs ---
    ASSERT_EQ(clone_tro->get_subcomponents().size(), 2u);

    auto* clone_root = clone_tro->get_root_component();
    expect_obj(clone_root, AID("root_component#3"));
    expect_flags_proto(clone_root);
    EXPECT_NE(clone_root, src_tro->get_root_component());
    EXPECT_EQ(clone_root->get_class_obj(), src_tro->get_root_component());

    auto* clone_tc = clone_tro->get_test_component();
    expect_obj(clone_tc, AID("test_component#3"));
    expect_flags_proto(clone_tc);
    EXPECT_NE(clone_tc, src_tro->get_test_component());
    EXPECT_EQ(clone_tc->get_class_obj(), src_tro->get_test_component());

    // --- tc: m_obj_instantiate cloned, m_obj_share shared ---
    auto* src_tc = src_tro->get_test_component();
    EXPECT_EQ(clone_tc->m_pod_instantiate, 42.0f);
    EXPECT_EQ(clone_tc->m_pod_share, 99.0f);
    expect_obj(clone_tc->m_obj_instantiate, AID("trc_ref_instantiate#3"));
    expect_flags_proto(clone_tc->m_obj_instantiate);
    EXPECT_NE(clone_tc->m_obj_instantiate, src_tc->m_obj_instantiate);
    EXPECT_EQ(clone_tc->m_obj_share, src_tc->m_obj_share);
}

// clone_obj from a proto with m_mode=instance_obj (instance clone)
//
// Same source as above, but clone gets instance flags.
//
// TO (instance clone, class_obj → src):
//   "clone_inst_dst"                                (inst, class_obj → "clone_inst_src")
//     m_obj_instantiate: → "tro_ref_instantiate#2"  (inst, cloned from #1)
//     m_obj_share:       → "tro_ref_share"          (ro, shared from src)
//     components:
//       ├─ "root_component#3"                       (inst, cloned from #2)
//       └─ "test_component#3" (test_root_component) (inst, cloned from #2)
//            m_obj_instantiate: → "trc_ref_instantiate#3"  (inst, cloned from #2)
//            m_obj_share:       → "trc_ref_share"          (ro, shared from src)
TEST_F(test_ctor, clone_obj_as_instance)
{
    auto olc = make_olc();

    root::test_root_object::construct_params params;
    auto src_result = core::object_constructor(&olc).construct_obj(
        AID("test_root_object"), AID("clone_inst_src"), params, true);
    ASSERT_TRUE(src_result.has_value());
    auto* src = src_result.value();
    auto* src_tro = src->as<root::test_root_object>();

    core::object_constructor ctor(&olc, core::object_load_type::instance_obj);
    auto clone_result = ctor.clone_obj(*src, AID("clone_inst_dst"));
    ASSERT_TRUE(clone_result.has_value());
    auto* clone = clone_result.value();
    auto* clone_tro = clone->as<root::test_root_object>();
    ASSERT_TRUE(clone_tro);

    // --- clone identity ---
    expect_obj(clone, AID("clone_inst_dst"));
    expect_flags_instance(clone);
    EXPECT_NE(clone, src);
    EXPECT_EQ(clone->get_class_obj(), src);

    // --- PODs copied ---
    EXPECT_EQ(clone_tro->m_pod_instantiate, 42.0f);
    EXPECT_EQ(clone_tro->m_pod_share, 99.0f);

    // --- m_obj_instantiate: new instance (clone_obj with instance mode) ---
    expect_obj(clone_tro->m_obj_instantiate, AID("tro_ref_instantiate#2"));
    expect_flags_instance(clone_tro->m_obj_instantiate);
    EXPECT_NE(clone_tro->m_obj_instantiate, src_tro->m_obj_instantiate);

    // --- m_obj_share: shared from src ---
    EXPECT_EQ(clone_tro->m_obj_share, src_tro->m_obj_share);

    // --- components: cloned with new IDs, instance flags ---
    ASSERT_EQ(clone_tro->get_subcomponents().size(), 2u);

    auto* clone_root = clone_tro->get_root_component();
    expect_obj(clone_root, AID("root_component#3"));
    expect_flags_instance(clone_root);
    EXPECT_NE(clone_root, src_tro->get_root_component());

    auto* clone_tc = clone_tro->get_test_component();
    expect_obj(clone_tc, AID("test_component#3"));
    expect_flags_instance(clone_tc);
    EXPECT_NE(clone_tc, src_tro->get_test_component());

    // --- tc: m_obj_instantiate cloned, m_obj_share shared ---
    auto* src_tc = src_tro->get_test_component();
    EXPECT_EQ(clone_tc->m_pod_instantiate, 42.0f);
    EXPECT_EQ(clone_tc->m_pod_share, 99.0f);
    expect_obj(clone_tc->m_obj_instantiate, AID("trc_ref_instantiate#3"));
    expect_flags_instance(clone_tc->m_obj_instantiate);
    EXPECT_NE(clone_tc->m_obj_instantiate, src_tc->m_obj_instantiate);
    EXPECT_EQ(clone_tc->m_obj_share, src_tc->m_obj_share);
}
