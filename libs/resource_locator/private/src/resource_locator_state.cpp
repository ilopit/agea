#include "resource_locator/resource_locator_state.h"

#include <resource_locator/resource_locator.h>

#include <global_state/global_state.h>

void
agea::state_mutator__resource_locator::set(gs::state& es)
{
    es.m_resource_locator = es.create_box<resource_locator>("resource_locator");
}
