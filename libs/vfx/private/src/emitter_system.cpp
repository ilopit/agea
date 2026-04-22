#include "vfx/emitter_system.h"

#include <utils/check.h>

#include <algorithm>

namespace kryga
{
namespace vfx
{

emitter*
emitter_system::create(std::string name, emitter_params params)
{
    m_entries.push_back({std::move(name), std::make_unique<emitter>(std::move(params))});
    return m_entries.back().e.get();
}

void
emitter_system::remove(emitter* e)
{
    KRG_check(e != nullptr, "null emitter");
    std::erase_if(m_entries, [e](const entry& entry) { return entry.e.get() == e; });
}

void
emitter_system::tick(float dt)
{
    for (auto& entry : m_entries)
    {
        entry.e->tick(dt);
    }
}

}  // namespace vfx
}  // namespace kryga
