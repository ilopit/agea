#pragma once

#include "core/object_load_context.h"

#include <memory>

namespace kryga
{
namespace core
{
class object_load_context_builder
{
public:
    // clang-format off
    object_load_context_builder& set_proto_local_set    (cache_set* v)                              { m_proto_local_set = v; return *this; }
    object_load_context_builder& set_instance_local_set (cache_set* v)                              { m_instance_local_set = v; return *this; }
    object_load_context_builder& set_level              (level* v)                                  { m_level = v; return *this; }
    object_load_context_builder& set_objects_mapping    (const std::shared_ptr<object_mapping>& v)  { m_objects_mapping = v; return *this; }
    object_load_context_builder& set_ownable_cache      (line_cache<root::smart_object_ptr>* v)     { m_ownable_cache = v; return *this; }
    object_load_context_builder& set_package            (package* v)                                { m_package = v; return *this; }
    object_load_context_builder& set_prefix_path        (const utils::path& v)                      { m_prefix_path = v; return *this; }
    // clang-format on

    std::unique_ptr<object_load_context>
    build()
    {
        auto ctx = std::make_unique<object_load_context>();

        ctx->m_proto_local_set = m_proto_local_set;
        ctx->m_instance_local_set = m_instance_local_set;
        ctx->m_level = m_level;
        ctx->m_package = m_package;
        ctx->m_ownable_cache_ptr = m_ownable_cache;

        if (m_objects_mapping)
        {
            ctx->m_path_resolver.set_objects_mapping(m_objects_mapping);
        }
        if (!m_prefix_path.empty())
        {
            ctx->m_path_resolver.set_prefix_path(m_prefix_path);
        }
        return ctx;
    }

private:
    cache_set* m_proto_local_set = nullptr;
    cache_set* m_instance_local_set = nullptr;
    level* m_level = nullptr;
    std::shared_ptr<object_mapping> m_objects_mapping;
    line_cache<root::smart_object_ptr>* m_ownable_cache = nullptr;
    package* m_package = nullptr;
    utils::path m_prefix_path;
};
}  // namespace core
}  // namespace kryga
