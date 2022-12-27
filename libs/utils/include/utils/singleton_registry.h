#pragma once

#include "utils/check.h"
#include "utils/line_conteiner.h"

#include "utils/base_singleton_instance.h"

#include <memory>

namespace agea
{

class singleton_registry
{
public:
    void
    add(base_singleton_instance* obj);

    void
    remove(base_singleton_instance* obj);

private:
    utils::line_conteiner<base_singleton_instance*> m_refs;
};

}  // namespace agea
