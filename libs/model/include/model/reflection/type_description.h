#pragma once

namespace agea::reflection
{

struct type_description
{
    type_description() = default;

    type_description(int t)
        : type_id(t)
    {
    }

    type_description(int t, bool ptr)
        : type_id(t)
        , is_ptr(ptr)
    {
    }

    type_description(int t, bool ptr, bool collection)
        : type_id(t)
        , is_ptr(ptr)
        , is_collection(collection)
    {
    }

    int type_id = -1;
    bool is_ptr = false;
    bool is_collection = false;
};
}  // namespace agea::reflection