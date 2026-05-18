#include "packages/root/model/test/test_root_component.h"

#include <core/object_constructor.h>
#include <core/package.h>
#include <core/model_system.h>
#include <global_state/global_state.h>
#include <utils/defines_utils.h>

namespace kryga::root
{

KRG_gen_class_cd_default(test_root_component);

bool
test_root_component::construct_default(construct_params& params)
{
    if (!base_class::construct(params))
    {
        return false;
    }

    m_pod_instantiate = 42.0f;
    m_pod_share = 99.0f;

    auto& olc = m_package->get_load_context();
    core::object_constructor ctor(&olc);

    auto r1 = ctor.construct_obj(
        AID("smart_object"), AID("trc_ref_instantiate"), smart_object::construct_params{}, true);
    auto r2 = ctor.construct_obj(
        AID("smart_object"), AID("trc_ref_share"), smart_object::construct_params{}, true);

    if (!r1 || !r2)
    {
        return false;
    }

    m_obj_instantiate = r1.value();
    m_obj_share = r2.value();

    return true;
}

bool
test_root_component::construct(construct_params& params)
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
                                 core::name_of{AID("trc_ref_instantiate")},
                                 smart_object::construct_params{},
                                 is_proto);
    if (!r1)
    {
        return false;
    }
    m_obj_instantiate = r1.value();

    m_obj_share = olc.find_obj(AID("trc_ref_share"));

    return true;
}

}  // namespace kryga::root
