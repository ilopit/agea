#include "core/queues.h"

#include <global_state/global_state.h>

namespace kryga
{

void
state_mutator__queues::set(gs::state& s)
{
    auto p = s.create_box<core::queues>("queues");
    s.m_queues = p;
}

}  // namespace kryga
