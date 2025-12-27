#pragma once

#include "utils/check.h"
#include "utils/line_container.h"

#include "utils/base_singleton_instance.h"

#include <memory>

namespace agea
{

class singleton_registry
{
public:
    ~singleton_registry();

    void
    add(base_singleton_instance* obj);

    void
    remove(base_singleton_instance* obj);

private:
    utils::line_container<base_singleton_instance*> m_refs;
};

}  // namespace agea
