#pragma once

namespace agea
{
class singleton_registry;

struct base_singleton_instance
{
    friend class singleton_registry;

protected:
    void
    add();

    void
    remove();

    virtual void
    reset() = 0;

    virtual ~base_singleton_instance();

    singleton_registry* m_reg = nullptr;
};
}  // namespace agea