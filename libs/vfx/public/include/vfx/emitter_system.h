#pragma once

#include "vfx/emitter.h"

#include <memory>
#include <string>
#include <vector>

namespace kryga
{
namespace vfx
{

class emitter_system
{
public:
    struct entry
    {
        std::string name;
        std::unique_ptr<emitter> e;
    };

    emitter_system() = default;
    ~emitter_system() = default;

    emitter*
    create(std::string name, emitter_params params);

    void
    remove(emitter* e);

    void
    tick(float dt);

    std::vector<entry>&
    entries()
    {
        return m_entries;
    }

    const std::vector<entry>&
    entries() const
    {
        return m_entries;
    }

private:
    std::vector<entry> m_entries;
};

}  // namespace vfx
}  // namespace kryga
