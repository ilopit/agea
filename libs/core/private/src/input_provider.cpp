#include "core/input_provider.h"

namespace kryga
{

namespace
{
core::input_provider* s_input_provider = nullptr;
}

core::input_provider*
glob::get_input_provider()
{
    return s_input_provider;
}

void
glob::set_input_provider(core::input_provider* p)
{
    s_input_provider = p;
}

}  // namespace kryga
