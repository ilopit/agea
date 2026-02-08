#pragma once

#include "packages/base/model/input_component.ar.h"

#include "packages/root/model/components/component.h"
#include <core/input_provider.h>

namespace kryga
{
namespace base
{

KRG_ar_class();
class input_component : public root::component
{
    KRG_gen_meta__input_component();

public:
    KRG_gen_class_meta(input_component, root::component);
    KRG_gen_construct_params{};
    KRG_gen_meta_api;

    bool
    construct(construct_params& c);

    core::input_provider*
    get_input_provider() const
    {
        return glob::get_input_provider();
    }
};

}  // namespace base
}  // namespace kryga
