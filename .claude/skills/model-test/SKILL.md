---
name: model-test
description: Write C++ GTest model-layer tests for the core object system (construct, clone, instantiate, save/load). Use when the user asks to add a model test, test object construction, test serialization, or verify object lifecycle.
argument-hint: "<description of what to test>"
allowed-tools: Read, Edit, Write, Grep, Glob, Bash, Agent
---

Write C++ GTests for the model layer in `packages/root/private/tests/model/`.

## Principles

- **Always write a layout comment before the test body.** Show what goes in (FROM) and what comes out (TO) — IDs, flags, which refs are shared vs new.
- **Group tests by API.** Each file covers one operation (construct, clone, instantiate, save/load). Pick the right file or create a new one if the operation doesn't fit.
- **Use fixture helpers for repetitive checks.** Identity + cache presence, flag verification — if you're writing the same EXPECT pattern more than once, it should be a helper on the fixture.
- **Verify saved YAML exactly.** Save tests compare the full serialized output as a string constant. The YAML is the source of truth.
- **Test both proto and instance variants.** Most operations behave differently for proto (readonly) and instance (mutable) objects.
- **Clean up VFS artifacts.** Tests that write files must remove them.
- **Negative tests go with the positive ones.**
- **Read existing tests first.** Match the style and patterns of nearby tests in the same file.

## Example

```cpp
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
    // ... construct proto, instantiate ...

    expect_obj(inst, AID("inst_from_proto"));
    expect_flags_instance(inst);
    EXPECT_EQ(inst->get_class_obj(), proto);
    EXPECT_NE(inst_tro->m_obj_instantiate, proto_tro->m_obj_instantiate);
    EXPECT_EQ(inst_tro->m_obj_share, proto_tro->m_obj_share);
    // ... verify components ...
}
```

## Reference

Property behavior table:
```
              | share         | instantiate
--------------+---------------+------------------
construct     | shared ptr    | new obj (name_of)
instantiate   | shared ptr    | new instance
clone         | shared ptr    | new clone
```

New `.cpp` files require `cmake --preset host` to reconfigure.
