#pragma once

namespace agea
{
class singleton_registry;

struct base_singleton_instance
{
protected:
    void
    add();

    void
    remove();

    virtual ~base_singleton_instance();

    singleton_registry* m_reg = nullptr;
};
}  // namespace agea