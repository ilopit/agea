#include "utils/base_singleton_instance.h"

#include "utils/singleton_registry.h"

namespace agea
{
void
base_singleton_instance::add()
{
    m_reg->add(this);
}

void
base_singleton_instance::remove()
{
    m_reg->remove(this);
}

base_singleton_instance::~base_singleton_instance()
{
}
}  // namespace agea