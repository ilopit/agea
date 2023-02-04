#include "utils/singleton_registry.h"

namespace agea
{

singleton_registry::~singleton_registry()
{
    for (auto r : m_refs)
    {
        r->reset();
    }
}

void
singleton_registry::add(base_singleton_instance* obj)
{
    auto itr = m_refs.find(obj);

    AGEA_check(itr == m_refs.end(), "Should be here");

    m_refs.emplace_back(obj);
}

void
singleton_registry::remove(base_singleton_instance* obj)
{
    auto itr = m_refs.find(obj);

    AGEA_check(itr != m_refs.end(), "Should not be here");
}

}  // namespace agea