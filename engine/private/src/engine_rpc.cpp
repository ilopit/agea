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

#include <vfs/vfs.h>

#include <utils/kryga_log.h>

#include <sol2_unofficial/sol.h>

#include <json/json.h>

#include <chrono>
#include <filesystem>
#include <string>

namespace kryga::engine_private
{

void
register_rpc_handlers(vulkan_engine& eng, rpc::rpc_server& server)
{
    // ping — pure echo, no state, no queue.
    server.on_request(
        "ping",
        [](const Json::Value& params, Json::Value& result, std::string&)
        { result = params; });

    // All other handlers go through the main-thread action queue. Reads use
    // wait_main_action (block I/O thread until main produces the result).
    // High-volume mutations (properties.set during drag-scrub) use
    // queue_main_action (fire-and-forget; client reconciles via
    // properties.changed). Bulk mutations (level.load, engine.setMode) also
    // fire-and-forget — they have observable effects via other notifications.

    server.on_request(
        "sync.reload",
        [&eng](const Json::Value& params, Json::Value& result, std::string& err)
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
                    else if (ext == "vert" || ext == "frag")
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
                        Json::Value node(Json::objectValue);
                        node["id"] = kv.first.str();
                        node["label"] = kv.first.str();
                        node["has_children"] = false;
                        children.append(std::move(node));
                    }
                    r["children"] = std::move(children);
                });
            if (!done) { err = "scene.getRoot timed out"; return; }
            result = r;
        });

    server.on_request(
        "scene.getChildren",
        [](const Json::Value& params, Json::Value& result, std::string& err)
        {
            // No state access — input validation only.
            if (!params.isObject() || !params.isMember("id"))
            {
                err = "missing 'id' parameter";
                return;
            }
            Json::Value r(Json::objectValue);
            r["children"] = Json::Value(Json::arrayValue);
            result = r;
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
                    auto* go = lvl->find_game_object(AID(id_str));
                    if (!go)
                    {
                        local_err = "game_object not found: " + id_str;
                        return;
                    }
                    r = engine_private::encode_game_object_properties(*go);
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

    // properties.set is fire-and-forget so high-volume drag-scrub doesn't
    // wedge the I/O thread one frame per sample. Echo via properties.changed
    // gives the client confirmation; on failure the engine logs (no error
    // notification today — TODO if it bites).
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
