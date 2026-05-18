#include "packages/root/model/test/test_root_object.h"
#include "packages/root/model/test/test_root_component.h"

#include <core/object_constructor.h>
#include <core/package.h>
#include <core/model_system.h>
#include <global_state/global_state.h>
#include <utils/defines_utils.h>

namespace kryga::root
{

KRG_gen_class_cd_default(test_root_object);

bool
test_root_object::construct_default(construct_params& params)
{
    if (!base_class::construct_default(params))
    {
        return false;
    }

    m_pod_instantiate = 42.0f;
    m_pod_share = 99.0f;

    auto& olc = m_package->get_load_context();
    core::object_constructor ctor(&olc);

    auto r1 = ctor.construct_obj(
        AID("smart_object"), AID("tro_ref_instantiate"), smart_object::construct_params{}, true);
    auto r2 = ctor.construct_obj(
        AID("smart_object"), AID("tro_ref_share"), smart_object::construct_params{}, true);

    if (!r1 || !r2)
    {
        return false;
    }

    m_obj_instantiate = r1.value();
    m_obj_share = r2.value();

    auto* tc = spawn_component<test_root_component>(
        get_root_component(), AID("test_component"), test_root_component::construct_params{});

    return tc != nullptr;
}

bool
test_root_object::construct(construct_params& params)
{
    if (!base_class::construct(params))
    {
        return false;
    }

    m_pod_instantiate = 42.0f;
    m_pod_share = 99.0f;

    bool is_proto = !get_flags().instance_obj;
    auto& olc = m_package->get_load_context();
    core::object_constructor ctor(&olc);

    auto r1 = ctor.construct_obj(AID("smart_object"),
                                 core::name_of{AID("tro_ref_instantiate")},
                                 smart_object::construct_params{},
                                 is_proto);
    if (!r1)
    {
        return false;
    }
    m_obj_instantiate = r1.value();

    m_obj_share = olc.find_obj(AID("tro_ref_share"));

    auto* tc = spawn_component<test_root_component>(
        get_root_component(), AID("test_component"), test_root_component::construct_params{});

    return tc != nullptr;
}

test_root_component*
test_root_object::add_test_component(component* parent, const utils::id& name)
{
    auto* tc =
        spawn_component<test_root_component>(parent, name, test_root_component::construct_params{});
    if (!tc)
    {
        return nullptr;
    }

    m_renderable_components.clear();
    recreate_structure_from_layout();
    return tc;
}

test_root_component*
test_root_object::get_test_component()
{
    for (auto* c : get_subcomponents())
    {
        if (auto* tc = c->as<test_root_component>())
        {
            return tc;
        }
    }
    return nullptr;
}

}  // namespace kryga::root
