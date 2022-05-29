#pragma once

#include "model/object_construction_context.h"

#include "model/smart_object.h"
#include "model/caches/class_object_cache.h"
#include "model/caches/objects_cache.h"

#include "utils/agea_log.h"

namespace agea
{
namespace model
{

object_constructor_context::object_constructor_context()
    : class_obj_cache(std::make_shared<class_objects_cache>())
    , instance_obj_cache(std::make_shared<objects_cache>())
{
    ALOG_TRACE("Created");
}

object_constructor_context::~object_constructor_context()
{
    ALOG_TRACE("Destructed");
}

std::shared_ptr<smart_object>
object_constructor_context::extract_last()
{
    AGEA_check(last_obj, "We can expect object here");

    auto last = last_obj;
    last_obj = nullptr;
    return last;
}

bool
object_constructor_context::propagate_to_io_cache()
{
    while (!temporary_obj_cache.empty())
    {
        auto& obj = temporary_obj_cache.back();

        ALOG_INFO("Obj {0} propagated to O cache", obj->get_id());

        instance_obj_cache->insert(std::move(obj));
        temporary_obj_cache.pop_back();
    }

    return true;
}

bool
object_constructor_context::propagate_to_co_cache()
{
    while (!temporary_obj_cache.empty())
    {
        auto& obj = temporary_obj_cache.back();

        ALOG_INFO("Obj {0} propagated to O cache", obj->get_id());

        class_obj_cache->insert(std::move(obj));
        temporary_obj_cache.pop_back();
    }

    return true;
}

}  // namespace model
}  // namespace agea