#include "engine/engine_counters.h"

#include <global_state/global_state.h>

namespace kryga
{
void
state_mutator__engine_counters::set(gs::state& s)
{
    auto p = s.create_box<engine_counters>("engine_counters");
    s.m_engine_counters = p;
}
}  // namespace kryga