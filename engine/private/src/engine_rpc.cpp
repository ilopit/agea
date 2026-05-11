#include "engine/private/engine_rpc.h"

#include "engine/kryga_engine.h"
#include "engine/editor.h"
#include "engine/private/property_rpc.h"

#include <rpc/rpc_server.h>

#include <core/level.h>
#include <core/level_manager.h>
#include <core/caches/caches_map.h>
#include <core/reflection/lua_api.h>
#include <core/reflection/reflection_type.h>
#include <core/architype.h>

#include <vulkan_render/types/vulkan_render_data.h>
#include <vulkan_render/kryga_render.h>
#include <vulkan_render/render_config.h>

#include <global_state/global_state.h>

#include <render_bridge/render_bridge.h>

#include <packages/root/model/assets/asset.h>
#include <packages/root/model/assets/shader_effect.h>
#include <packages/root/model/game_object.h>
#include <packages/root/model/components/component.h>

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
                    core::spawn_parameters sp;
                    sp.position = go->get_position();
                    auto* clone = lvl->spawn_object_as_clone<root::game_object>(
                        go->get_id(), gen_id, sp);
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

    // =========================================================================
    // Transform API
    // =========================================================================

    server.on_request(
        "transform.get",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            std::string local_err;
            Json::Value r(Json::objectValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().get_current_level();
                    if (!lvl) { local_err = "no level loaded"; return; }
                    auto* obj = lvl->find_object(AID(id_str));
                    if (!obj) { local_err = "object not found: " + id_str; return; }

                    root::game_object_component* goc = nullptr;
                    if (auto* go = obj->as<root::game_object>())
                        goc = go->get_root_component();
                    else if (auto* comp = obj->as<root::component>())
                        goc = dynamic_cast<root::game_object_component*>(comp);

                    if (!goc) { local_err = "object has no transform"; return; }

                    auto pos = goc->get_position();
                    auto rot = goc->get_rotation();
                    auto scl = goc->get_scale();

                    Json::Value jp(Json::arrayValue);
                    jp.append(pos.x); jp.append(pos.y); jp.append(pos.z);
                    r["position"] = jp;

                    Json::Value jr(Json::arrayValue);
                    jr.append(rot.x); jr.append(rot.y); jr.append(rot.z);
                    r["rotation"] = jr;

                    Json::Value js(Json::arrayValue);
                    js.append(scl.x); js.append(scl.y); js.append(scl.z);
                    r["scale"] = js;
                });
            if (!done) { err = "transform.get timed out"; return; }
            if (!local_err.empty()) { err = std::move(local_err); return; }
            result = r;
        });

    server.on_request(
        "transform.set",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            Json::Value pos_val = params.get("position", Json::nullValue);
            Json::Value rot_val = params.get("rotation", Json::nullValue);
            Json::Value scl_val = params.get("scale", Json::nullValue);
            std::string local_err;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().get_current_level();
                    if (!lvl) { local_err = "no level loaded"; return; }
                    auto* obj = lvl->find_object(AID(id_str));
                    if (!obj) { local_err = "object not found: " + id_str; return; }

                    root::game_object_component* goc = nullptr;
                    if (auto* go = obj->as<root::game_object>())
                        goc = go->get_root_component();
                    else if (auto* comp = obj->as<root::component>())
                        goc = dynamic_cast<root::game_object_component*>(comp);

                    if (!goc) { local_err = "object has no transform"; return; }

                    if (pos_val.isArray() && pos_val.size() == 3)
                        goc->set_position({pos_val[0].asFloat(),
                                           pos_val[1].asFloat(),
                                           pos_val[2].asFloat()});
                    if (rot_val.isArray() && rot_val.size() == 3)
                        goc->set_rotation({rot_val[0].asFloat(),
                                           rot_val[1].asFloat(),
                                           rot_val[2].asFloat()});
                    if (scl_val.isArray() && scl_val.size() == 3)
                        goc->set_scale({scl_val[0].asFloat(),
                                        scl_val[1].asFloat(),
                                        scl_val[2].asFloat()});
                });
            if (!done) { err = "transform.set timed out"; return; }
            if (!local_err.empty()) { err = std::move(local_err); return; }
            result = Json::Value(Json::objectValue);
        });

    // =========================================================================
    // Component API
    // =========================================================================

    server.on_request(
        "component.listTypes",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            Json::Value r(Json::arrayValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* rm = glob::glob_state().get_rm();
                    if (!rm) return;
                    for (auto& [name, rt] : rm->get_types_to_name())
                    {
                        if (rt->arch != core::architype::component)
                            continue;
                        Json::Value entry(Json::objectValue);
                        entry["type_id"] = rt->type_name.str();
                        r.append(std::move(entry));
                    }
                });
            if (!done) { err = "component.listTypes timed out"; return; }
            result = r;
        });

    server.on_request(
        "component.add",
        [&eng, &server](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() ||
                !params.isMember("object_id") ||
                !params.isMember("type_id"))
            {
                err = "missing 'object_id' or 'type_id' parameter";
                return;
            }
            std::string object_id = params["object_id"].asString();
            std::string type_id = params["type_id"].asString();
            std::string comp_name = params.get("name", type_id).asString();
            std::string local_err;
            std::string new_id;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().get_current_level();
                    if (!lvl) { local_err = "no level loaded"; return; }
                    auto* go = lvl->find_game_object(AID(object_id));
                    if (!go) { local_err = "game_object not found: " + object_id; return; }
                    auto* parent = go->get_root_component();
                    if (!parent) { local_err = "game_object has no root component"; return; }
                    root::component::construct_params cp;
                    auto* comp = go->spawn_component(
                        parent, AID(type_id), AID(comp_name), cp);
                    if (!comp) { local_err = "failed to spawn component"; return; }
                    new_id = comp->get_id().str();
                    server.notify("scene.changed", Json::Value(Json::objectValue));
                });
            if (!done) { err = "component.add timed out"; return; }
            if (!local_err.empty()) { err = std::move(local_err); return; }
            Json::Value r(Json::objectValue);
            r["id"] = new_id;
            result = r;
        });

    // =========================================================================
    // Level API
    // =========================================================================

    server.on_request(
        "level.save",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            std::string local_err;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().get_current_level();
                    if (!lvl) { local_err = "no level loaded"; return; }
                    auto& vfs = glob::glob_state().getr_vfs();
                    auto levels_path = vfs.real_path(vfs::rid("data://levels"));
                    if (!levels_path)
                    {
                        local_err = "cannot resolve levels path";
                        return;
                    }
                    kryga::utils::path save_path(levels_path.value());
                    if (!core::level_manager::save_level(*lvl, save_path))
                        local_err = "save_level failed";
                });
            if (!done) { err = "level.save timed out"; return; }
            if (!local_err.empty()) { err = std::move(local_err); return; }
            Json::Value r(Json::objectValue);
            r["ok"] = true;
            result = r;
        });

    // =========================================================================
    // Visibility / Layer API
    // =========================================================================

    // =========================================================================
    // Render Config API
    // =========================================================================

    server.on_request(
        "render_config.get",
        [&eng](const Json::Value&, Json::Value& result, std::string& err)
        {
            Json::Value r(Json::objectValue);
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto& cfg = glob::glob_state()
                                    .getr_vulkan_render()
                                    .get_render_config();

                    // Shadows
                    Json::Value sh(Json::objectValue);
                    sh["enabled"] = cfg.shadows.enabled;
                    sh["pcf"] = static_cast<int>(cfg.shadows.pcf);

                    const char* pcf_names[] = {
                        "pcf_3x3", "pcf_5x5", "pcf_7x7", "poisson16", "poisson32"};
                    int pcf_idx = static_cast<int>(cfg.shadows.pcf);
                    sh["pcf_name"] = (pcf_idx >= 0 && pcf_idx < 5)
                                         ? pcf_names[pcf_idx] : "unknown";

                    sh["bias"] = cfg.shadows.bias;
                    sh["normal_bias"] = cfg.shadows.normal_bias;
                    sh["cascade_count"] = cfg.shadows.cascade_count;
                    sh["distance"] = cfg.shadows.distance;
                    sh["map_size"] = cfg.shadows.map_size;
                    r["shadows"] = sh;

                    // Clusters
                    Json::Value cl(Json::objectValue);
                    cl["tile_size"] = cfg.clusters.tile_size;
                    cl["depth_slices"] = cfg.clusters.depth_slices;
                    cl["max_lights_per_cluster"] = cfg.clusters.max_lights_per_cluster;
                    r["clusters"] = cl;

                    // Lighting
                    Json::Value lt(Json::objectValue);
                    lt["directional_enabled"] = cfg.lighting.directional_enabled;
                    lt["local_enabled"] = cfg.lighting.local_enabled;
                    lt["baked_enabled"] = cfg.lighting.baked_enabled;
                    r["lighting"] = lt;

                    // Debug
                    Json::Value db(Json::objectValue);
                    db["editor_mode"] = cfg.debug.editor_mode;
                    db["show_grid"] = cfg.debug.show_grid;
                    db["light_wireframe"] = cfg.debug.light_wireframe;
                    db["light_icons"] = cfg.debug.light_icons;
                    db["frustum_culling"] = cfg.debug.frustum_culling;
                    r["debug"] = db;

                    // Render scale
                    Json::Value rs(Json::objectValue);
                    rs["enabled"] = cfg.render_scale.enabled;
                    rs["divisor"] = cfg.render_scale.divisor;
                    r["render_scale"] = rs;

                    // Outline
                    Json::Value ol(Json::objectValue);
                    ol["enabled"] = cfg.outline.enabled;
                    Json::Value color(Json::arrayValue);
                    for (int i = 0; i < 4; ++i) color.append(cfg.outline.color[i]);
                    ol["color"] = color;
                    ol["depth_threshold"] = cfg.outline.depth_threshold;
                    ol["normal_threshold"] = cfg.outline.normal_threshold;
                    r["outline"] = ol;
                });
            if (!done) { err = "render_config.get timed out"; return; }
            result = r;
        });

    server.on_request(
        "render_config.set",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject())
            {
                err = "params must be an object";
                return;
            }
            std::string local_err;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto& cfg = glob::glob_state()
                                    .getr_vulkan_render()
                                    .get_pending_render_config();

                    // Shadows
                    if (params.isMember("shadows"))
                    {
                        auto& s = params["shadows"];
                        if (s.isMember("enabled"))
                            cfg.shadows.enabled = s["enabled"].asBool();
                        if (s.isMember("pcf"))
                        {
                            auto& v = s["pcf"];
                            if (v.isString())
                            {
                                std::string name = v.asString();
                                if (name == "pcf_3x3") cfg.shadows.pcf = render::pcf_mode::pcf_3x3;
                                else if (name == "pcf_5x5") cfg.shadows.pcf = render::pcf_mode::pcf_5x5;
                                else if (name == "pcf_7x7") cfg.shadows.pcf = render::pcf_mode::pcf_7x7;
                                else if (name == "poisson16") cfg.shadows.pcf = render::pcf_mode::poisson16;
                                else if (name == "poisson32") cfg.shadows.pcf = render::pcf_mode::poisson32;
                                else { local_err = "unknown pcf mode: " + name; return; }
                            }
                            else
                            {
                                cfg.shadows.pcf = static_cast<render::pcf_mode>(v.asInt());
                            }
                        }
                        if (s.isMember("bias"))
                            cfg.shadows.bias = s["bias"].asFloat();
                        if (s.isMember("normal_bias"))
                            cfg.shadows.normal_bias = s["normal_bias"].asFloat();
                        if (s.isMember("cascade_count"))
                            cfg.shadows.cascade_count = s["cascade_count"].asUInt();
                        if (s.isMember("distance"))
                            cfg.shadows.distance = s["distance"].asFloat();
                        if (s.isMember("map_size"))
                            cfg.shadows.map_size = s["map_size"].asUInt();
                    }

                    // Clusters
                    if (params.isMember("clusters"))
                    {
                        auto& c = params["clusters"];
                        if (c.isMember("tile_size"))
                            cfg.clusters.tile_size = c["tile_size"].asUInt();
                        if (c.isMember("depth_slices"))
                            cfg.clusters.depth_slices = c["depth_slices"].asUInt();
                        if (c.isMember("max_lights_per_cluster"))
                            cfg.clusters.max_lights_per_cluster = c["max_lights_per_cluster"].asUInt();
                    }

                    // Lighting
                    if (params.isMember("lighting"))
                    {
                        auto& l = params["lighting"];
                        if (l.isMember("directional_enabled"))
                            cfg.lighting.directional_enabled = l["directional_enabled"].asBool();
                        if (l.isMember("local_enabled"))
                            cfg.lighting.local_enabled = l["local_enabled"].asBool();
                        if (l.isMember("baked_enabled"))
                            cfg.lighting.baked_enabled = l["baked_enabled"].asBool();
                    }

                    // Debug
                    if (params.isMember("debug"))
                    {
                        auto& d = params["debug"];
                        if (d.isMember("editor_mode"))
                            cfg.debug.editor_mode = d["editor_mode"].asBool();
                        if (d.isMember("show_grid"))
                            cfg.debug.show_grid = d["show_grid"].asBool();
                        if (d.isMember("light_wireframe"))
                            cfg.debug.light_wireframe = d["light_wireframe"].asBool();
                        if (d.isMember("light_icons"))
                            cfg.debug.light_icons = d["light_icons"].asBool();
                        if (d.isMember("frustum_culling"))
                            cfg.debug.frustum_culling = d["frustum_culling"].asBool();
                    }

                    // Render scale
                    if (params.isMember("render_scale"))
                    {
                        auto& rs = params["render_scale"];
                        if (rs.isMember("enabled"))
                            cfg.render_scale.enabled = rs["enabled"].asBool();
                        if (rs.isMember("divisor"))
                            cfg.render_scale.divisor = rs["divisor"].asUInt();
                    }

                    // Outline
                    if (params.isMember("outline"))
                    {
                        auto& o = params["outline"];
                        if (o.isMember("enabled"))
                            cfg.outline.enabled = o["enabled"].asBool();
                        if (o.isMember("color") && o["color"].isArray() && o["color"].size() == 4)
                        {
                            for (int i = 0; i < 4; ++i)
                                cfg.outline.color[i] = o["color"][i].asFloat();
                        }
                        if (o.isMember("depth_threshold"))
                            cfg.outline.depth_threshold = o["depth_threshold"].asFloat();
                        if (o.isMember("normal_threshold"))
                            cfg.outline.normal_threshold = o["normal_threshold"].asFloat();
                    }

                    cfg.validate();
                });
            if (!done) { err = "render_config.set timed out"; return; }
            if (!local_err.empty()) { err = std::move(local_err); return; }
            result = Json::Value(Json::objectValue);
            result["ok"] = true;
        });

    server.on_request(
        "visibility.set",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
        {
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            std::string id_str = params["id"].asString();
            bool visible = params.get("visible", true).asBool();
            std::string local_err;
            bool done = eng.wait_main_action(
                [&]()
                {
                    auto* lvl = glob::glob_state().get_current_level();
                    if (!lvl) { local_err = "no level loaded"; return; }
                    auto* obj = lvl->find_object(AID(id_str));
                    if (!obj) { local_err = "object not found: " + id_str; return; }

                    root::game_object_component* goc = nullptr;
                    if (auto* go = obj->as<root::game_object>())
                        goc = go->get_root_component();
                    else if (auto* comp = obj->as<root::component>())
                        goc = dynamic_cast<root::game_object_component*>(comp);

                    if (!goc) { local_err = "object has no layer flags"; return; }
                    goc->set_layer_flag(render::LAYER_VISIBLE, visible);
                    goc->mark_render_dirty();
                });
            if (!done) { err = "visibility.set timed out"; return; }
            if (!local_err.empty()) { err = std::move(local_err); return; }
            result = Json::Value(Json::objectValue);
        });
}

}  // namespace kryga::engine_private
