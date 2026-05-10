#include "engine/private/engine_rpc.h"

#include "engine/kryga_engine.h"
#include "engine/editor.h"
#include "engine/private/property_rpc.h"

#include <rpc/rpc_server.h>

#include <core/level.h>
#include <core/caches/caches_map.h>
#include <core/reflection/lua_api.h>

#include <global_state/global_state.h>

#include <render_bridge/render_bridge.h>

#include <packages/root/model/assets/asset.h>
#include <packages/root/model/assets/shader_effect.h>
#include <packages/root/model/game_object.h>
#include <packages/root/model/components/component.h>
#include <core/reflection/reflection_type.h>

#include <vfs/vfs.h>

#include <utils/kryga_log.h>

#include <sol2_unofficial/sol.h>

#include <json/json.h>

#include <chrono>
#include <filesystem>
#include <string>

namespace kryga::engine_private
{

// Shader diagnostic protocol: the render layer emits "diagnostics.shader"
// notifications with {diagnostics: [{file, line, column, severity, message}]}
// when shader compilation fails. "diagnostics.clear" clears them on success.
// See shader_compiler.h for the structured error type.

void
register_rpc_handlers(vulkan_engine& eng, rpc::rpc_server& server)
{
    server.on_request(
        "ping",
        [](const Json::Value& params, Json::Value& result, std::string&)
        { result = params; });

    server.on_request(
        "sync.reload",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("path"))
            {
                err = "missing 'path' parameter";
                return;
            }
            std::string path_str = params["path"].asString();
            std::string out;
            bool done = eng.wait_main_action(
                [&]()
                {
                    utils::path p(path_str);
                    std::string name, ext;
                    p.parse_file_name_and_ext(name, ext);

                    if (ext == "lua")
                    {
                        auto lua = glob::glob_state().get_lua();
                        auto lua_r = lua->state().script_file(p.str());
                        out = lua->buffer();
                        lua->reset();
                        if (lua_r.status() != sol::call_status::ok)
                        {
                            sol::error e = lua_r;
                            out += e.what();
                        }
                    }
                    else if (ext == "vert" || ext == "frag" || ext == "comp")
                    {
                        auto sec = glob::glob_state().get_class_shader_effects_cache();
                        auto ptr = sec->get_item(AID(name));
                        if (!ptr)
                        {
                            out = "shader effect not found: " + name;
                            return;
                        }
                        ptr->mark_render_dirty();
                        auto dep = glob::glob_state().getr_render_bridge()
                                       .get_dependency().get_node(ptr);
                        for (auto o : dep.m_children)
                        {
                            auto mt = o->as<root::asset>();
                            KRG_check(mt, "shader dep child is not an asset");
                            mt->mark_render_dirty();
                        }
                        // Clear previous diagnostics on successful reload
                        Json::Value clear(Json::objectValue);
                        server.notify("diagnostics.clear", clear);
                    }
                    else if (ext == "glsl")
                    {
                        // Shared includes changed — mark all shader effects dirty.
                        auto sec = glob::glob_state().get_class_shader_effects_cache();
                        for (auto& [id, obj] : sec->get_items())
                        {
                            auto se = obj->as<root::shader_effect>();
                            if (se) se->mark_render_dirty();
                        }
                        auto& dep = glob::glob_state().getr_render_bridge().get_dependency();
                        for (auto& [id, obj] : sec->get_items())
                        {
                            auto node = dep.get_node(obj);
                            for (auto o : node.m_children)
                            {
                                auto mt = o->as<root::asset>();
                                if (mt) mt->mark_render_dirty();
                            }
                        }
                        Json::Value clear(Json::objectValue);
                        server.notify("diagnostics.clear", clear);
                    }
                    else
                    {
                        out = "unsupported file extension: " + ext;
                    }
                },
                std::chrono::seconds(5));
            if (!done)
            {
                err = "sync.reload timed out after 5s";
                return;
            }
            Json::Value r(Json::objectValue);
            r["result"] = out;
            result = r;
        });

    server.on_request(
        "selection.get",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            Json::Value r(Json::objectValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto sel = glob::glob_state().get_game_editor()->get_selected();
                    r["id"] = sel.valid() ? sel.str() : std::string();
                });
            if (!done) { err = "selection.get timed out"; return; }
            result = r;
        });

    server.on_request(
        "selection.set",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            bool done = eng.wait_main_action(
                [&server, id_str]()
                {
                    utils::id new_sel =
                        id_str.empty() ? utils::id() : AID(id_str);
                    glob::glob_state().get_game_editor()->set_selected(new_sel);
                    Json::Value note(Json::objectValue);
                    note["id"] = id_str;
                    server.notify("selection.changed", note);
                });
            if (!done) { err = "selection.set timed out"; return; }
            result = Json::Value(Json::objectValue);
        });

    server.on_request(
        "scene.getRoot",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            Json::Value r(Json::objectValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto lvl = glob::glob_state().get_current_level();
                    if (!lvl)
                    {
                        r["level"] = std::string();
                        r["children"] = Json::Value(Json::arrayValue);
                        return;
                    }
                    r["level"] = lvl->get_id().str();

                    Json::Value children(Json::arrayValue);
                    for (const auto& kv : lvl->get_game_objects().get_items())
                    {
                        auto* go = kv.second->as<root::game_object>();
                        Json::Value node(Json::objectValue);
                        node["id"] = kv.first.str();
                        node["label"] = kv.first.str();
                        node["kind"] = "game_object";
                        bool has_components = go &&
                            !go->get_subcomponents().empty();
                        node["has_children"] = has_components;
                        if (go && go->get_reflection())
                        {
                            node["type_name"] = go->get_reflection()->type_name.str();
                        }
                        children.append(std::move(node));
                    }
                    r["children"] = std::move(children);
                });
            if (!done) { err = "scene.getRoot timed out"; return; }
            result = r;
        });

    server.on_request(
        "scene.getChildren",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            Json::Value r(Json::objectValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    r["children"] = Json::Value(Json::arrayValue);
                    auto* lvl = glob::glob_state().get_current_level();
                    if (!lvl) return;

                    auto make_component_node = [](root::component* comp) -> Json::Value
                    {
                        Json::Value node(Json::objectValue);
                        node["id"] = comp->get_id().str();
                        std::string label = comp->get_id().str();
                        if (comp->get_reflection())
                            label += " (" + comp->get_reflection()->type_name.str() + ")";
                        node["label"] = label;
                        node["kind"] = "component";
                        node["has_children"] = !comp->get_children().empty();
                        if (comp->get_reflection())
                            node["type_name"] = comp->get_reflection()->type_name.str();
                        return node;
                    };

                    auto* obj = lvl->find_object(AID(id_str));
                    if (!obj) return;

                    Json::Value children(Json::arrayValue);
                    if (auto* go = obj->as<root::game_object>())
                    {
                        for (auto* comp : go->get_subcomponents())
                        {
                            if (comp->get_parent_idx() != root::NO_parent)
                                continue;
                            children.append(make_component_node(comp));
                        }
                    }
                    else if (auto* comp = obj->as<root::component>())
                    {
                        for (auto* child : comp->get_children())
                            children.append(make_component_node(child));
                    }
                    r["children"] = std::move(children);
                });
            if (!done) { err = "scene.getChildren timed out"; return; }
            result = r;
        });

    server.on_request(
        "scene.create",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("name"))
            {
                err = "missing 'name' parameter";
                return;
            }
            std::string name = params["name"].asString();
            std::string local_err;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().get_current_level();
                    if (!lvl) { local_err = "no level loaded"; return; }
                    root::game_object::construct_params cp;
                    auto* go = lvl->spawn_object<root::game_object>(AID(name), cp);
                    if (!go) { local_err = "spawn_object failed"; return; }
                    server.notify("scene.changed", Json::Value(Json::objectValue));
                });
            if (!done) { err = "scene.create timed out"; return; }
            if (!local_err.empty()) { err = std::move(local_err); return; }
            Json::Value r(Json::objectValue);
            r["id"] = name;
            result = r;
        });

    server.on_request(
        "scene.delete",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            std::string local_err;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().get_current_level();
                    if (!lvl) { local_err = "no level loaded"; return; }
                    auto* go = lvl->find_game_object(AID(id_str));
                    if (!go) { local_err = "game_object not found: " + id_str; return; }
                    auto& cache = lvl->get_game_objects();
                    cache.remove_item(*go);
                    server.notify("scene.changed", Json::Value(Json::objectValue));
                });
            if (!done) { err = "scene.delete timed out"; return; }
            if (!local_err.empty()) { err = std::move(local_err); return; }
            result = Json::Value(Json::objectValue);
        });

    server.on_request(
        "scene.duplicate",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            std::string local_err;
            std::string new_id;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().get_current_level();
                    if (!lvl) { local_err = "no level loaded"; return; }
                    auto* go = lvl->find_game_object(AID(id_str));
                    if (!go) { local_err = "game_object not found: " + id_str; return; }
                    auto gen_id = glob::glob_state().get_id_generator()->generate(AID(id_str));
                    root::game_object::construct_params cp;
                    cp.pos = go->get_position();
                    auto* clone = lvl->spawn_object<root::game_object>(gen_id, cp);
                    if (!clone) { local_err = "duplicate failed"; return; }
                    new_id = gen_id.str();
                    server.notify("scene.changed", Json::Value(Json::objectValue));
                });
            if (!done) { err = "scene.duplicate timed out"; return; }
            if (!local_err.empty()) { err = std::move(local_err); return; }
            Json::Value r(Json::objectValue);
            r["id"] = new_id;
            result = r;
        });

    server.on_request(
        "scene.rename",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id") || !params.isMember("name"))
            {
                err = "missing 'id' or 'name' parameter";
                return;
            }
            // Rename is not trivially supported in the object model (id is immutable
            // in the cache). Return a clear error until the engine supports it.
            err = "rename not yet supported by the engine object model";
        });

    server.on_request(
        "properties.get",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            std::string local_err;
            Json::Value r;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().get_current_level();
                    if (!lvl) { local_err = "no level loaded"; return; }
                    auto* obj = lvl->find_object(AID(id_str));
                    if (!obj) { local_err = "object not found: " + id_str; return; }
                    if (auto* go = obj->as<root::game_object>())
                        r = engine_private::encode_game_object_properties(*go);
                    else
                        r = engine_private::encode_component_properties(
                            *static_cast<root::component*>(obj));
                });
            if (!done) { err = "properties.get timed out"; return; }
            if (!local_err.empty()) { err = std::move(local_err); return; }
            result = r;
        });

    server.on_request(
        "level.list",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            Json::Value r(Json::objectValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    Json::Value arr(Json::arrayValue);
                    auto& vfs = glob::glob_state().getr_vfs();
                    auto p = vfs.real_path(vfs::rid("data", "levels"));
                    if (p && std::filesystem::exists(*p))
                    {
                        const std::string ext = ".alvl";
                        for (const auto& entry :
                             std::filesystem::directory_iterator(*p))
                        {
                            auto name = entry.path().filename().string();
                            if (name.size() > ext.size() &&
                                name.compare(name.size() - ext.size(),
                                             ext.size(), ext) == 0)
                            {
                                arr.append(name.substr(0, name.size() - ext.size()));
                            }
                        }
                    }
                    r["levels"] = arr;
                    if (auto* lvl = glob::glob_state().get_current_level())
                    {
                        r["current"] = lvl->get_id().str();
                    }
                    else
                    {
                        r["current"] = std::string();
                    }
                });
            if (!done) { err = "level.list timed out"; return; }
            result = r;
        });

    server.on_request(
        "level.load",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            eng.queue_main_action(
                [&eng, id_str]()
                {
                    if (!eng.load_level(AID(id_str)))
                    {
                        ALOG_WARN("level.load: failed to load '{}'", id_str);
                    }
                });
            Json::Value r(Json::objectValue);
            r["queued"] = true;
            result = r;
        });

    server.on_request(
        "engine.getMode",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            Json::Value r(Json::objectValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto mode = glob::glob_state().get_game_editor()->get_mode();
                    r["mode"] =
                        (mode == engine::editor_mode::playing)
                            ? std::string("play") : std::string("edit");
                });
            if (!done) { err = "engine.getMode timed out"; return; }
            result = r;
        });

    server.on_request(
        "engine.setMode",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("mode"))
            {
                err = "missing 'mode' parameter (expect 'edit' or 'play')";
                return;
            }
            std::string m = params["mode"].asString();
            if (m != "edit" && m != "play")
            {
                err = "mode must be 'edit' or 'play', got: " + m;
                return;
            }
            eng.queue_main_action(
                [m]()
                {
                    auto* ge = glob::glob_state().get_game_editor();
                    auto current = ge->get_mode();
                    if (m == "play" && current != engine::editor_mode::playing)
                    {
                        ge->enter_play_mode();
                    }
                    else if (m == "edit" && current == engine::editor_mode::playing)
                    {
                        ge->exit_play_mode();
                    }
                });
            Json::Value r(Json::objectValue);
            r["queued"] = true;
            result = r;
        });

    server.on_request(
        "engine.shutdown",
        [&eng](const Json::Value&, Json::Value& result, std::string&)
        {
            eng.request_shutdown();
            result = Json::Value(Json::objectValue);
            result["ok"] = true;
        });

    server.on_request(
        "properties.set",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() ||
                !params.isMember("owner_id") ||
                !params.isMember("name") ||
                !params.isMember("value"))
            {
                err = "missing 'owner_id'/'name'/'value' parameter";
                return;
            }
            std::string owner_id = params["owner_id"].asString();
            std::string name = params["name"].asString();
            Json::Value value = params["value"];
            eng.queue_main_action(
                [&server, owner_id, name, value]()
                {
                    auto* owner = engine_private::find_owner(owner_id);
                    if (!owner)
                    {
                        ALOG_WARN("properties.set: owner not found: {}", owner_id);
                        return;
                    }
                    Json::Value canonical;
                    std::string set_err = engine_private::set_owner_field(
                        *owner, name, value, canonical);
                    if (!set_err.empty())
                    {
                        ALOG_WARN("properties.set: {}", set_err);
                        return;
                    }
                    Json::Value note(Json::objectValue);
                    note["owner_id"] = owner_id;
                    note["name"] = name;
                    note["value"] = canonical;
                    server.notify("properties.changed", note);
                });
            Json::Value r(Json::objectValue);
            r["queued"] = true;
            result = r;
        });
}

}  // namespace kryga::engine_private
