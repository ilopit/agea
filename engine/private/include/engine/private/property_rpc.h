#pragma once

// Reflection-driven JSON encoding/decoding for the editor RPC. Walks
// reflection_type::m_editor_properties and converts known primitive types
// (bool/i*/u*/f*/string/id/vec*) to JSON. Unknown types fall back to
// to_string_handler with readonly=true.

#include <json/json.h>

#include <string>

namespace kryga
{
namespace root
{
class smart_object;
class game_object;
class component;
}  // namespace root

namespace engine_private
{

// Encode a single smart_object's properties for the inspector/editor.
Json::Value
encode_owner(root::smart_object& obj);

// Build the inspector payload for a game_object: own properties plus a
// flattened list of its component tree, each owner stamped with its id and
// reflection type name.
//
// Shape:
//   {
//     "id": "<game_object id>",
//     "owners": [
//       { "id": "<owner id>", "type": "<rt name>",
//         "categories": [ { "name": "...",
//                           "fields": [ {"name":..,"kind":..,"value":..,
//                                        "readonly":...} ] } ] },
//       ...
//     ]
//   }
Json::Value
encode_game_object_properties(root::game_object& go);

// Build the inspector payload for a single component.
Json::Value
encode_component_properties(root::component& comp);

// Locate an inspectable owner (game_object or component) by id in the
// current level. Returns null if not found.
root::smart_object*
find_owner(const std::string& id_str);

// Set a single field on the owner. Returns empty string on success, error
// message otherwise. On success writes the canonical JSON value back into
// out_value (echoed via properties.changed).
std::string
set_owner_field(root::smart_object& owner,
                const std::string& field_name,
                const Json::Value& value,
                Json::Value& out_value);

}  // namespace engine_private
}  // namespace kryga
