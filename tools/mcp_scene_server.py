#!/usr/bin/env python3
"""
MCP server that bridges Claude to the Kryga engine's RPC.

Exposes scene manipulation, transform, component, property, level, and
visibility RPCs as MCP tools. Connects to the engine via TCP using the
same JSON-RPC 2.0 + Content-Length framing protocol as the VS Code extension.

Usage:
    python tools/mcp_scene_server.py
"""

import json
import socket
import sys
from pathlib import Path
from typing import Any

# MCP SDK
from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Tool, TextContent

# ---------------------------------------------------------------------------
# Engine RPC client (persistent connection)
# ---------------------------------------------------------------------------

PROJECT_ROOT = Path(__file__).parent.parent


class EngineRPC:
    def __init__(self):
        self._sock: socket.socket | None = None
        self._buf = b""
        self._next_id = 1

    def _discovery_path(self) -> Path:
        return PROJECT_ROOT / "tmp" / "editor_rpc.json"

    def _ensure_connected(self) -> None:
        if self._sock is not None:
            return
        p = self._discovery_path()
        if not p.exists():
            raise ConnectionError(
                f"Engine not running (no discovery file at {p})")
        info = json.loads(p.read_text())
        port = info["port"]
        self._sock = socket.create_connection(("127.0.0.1", port), timeout=10.0)
        self._sock.settimeout(10.0)
        self._buf = b""

    def _send(self, msg: dict) -> None:
        body = json.dumps(msg).encode("utf-8")
        self._sock.sendall(
            f"Content-Length: {len(body)}\r\n\r\n".encode("ascii") + body)

    def _recv_response(self, want_id: int) -> dict:
        while True:
            while b"\r\n\r\n" not in self._buf:
                chunk = self._sock.recv(8192)
                if not chunk:
                    self._sock = None
                    raise ConnectionError("Engine closed connection")
                self._buf += chunk
            head, _, rest = self._buf.partition(b"\r\n\r\n")
            length = 0
            for line in head.split(b"\r\n"):
                if line.lower().startswith(b"content-length:"):
                    length = int(line.split(b":", 1)[1].strip())
            while len(rest) < length:
                chunk = self._sock.recv(length - len(rest))
                if not chunk:
                    self._sock = None
                    raise ConnectionError("Engine closed mid-frame")
                rest += chunk
            msg = json.loads(rest[:length].decode("utf-8"))
            self._buf = rest[length:]
            if msg.get("id") == want_id:
                return msg
            # Skip notifications

    def call(self, method: str, params: dict | None = None) -> Any:
        self._ensure_connected()
        req_id = self._next_id
        self._next_id += 1
        try:
            self._send({
                "jsonrpc": "2.0",
                "id": req_id,
                "method": method,
                "params": params or {}
            })
            resp = self._recv_response(req_id)
        except (ConnectionError, OSError, socket.timeout):
            self._sock = None
            raise
        if "error" in resp:
            raise RuntimeError(resp["error"].get("message", str(resp["error"])))
        return resp.get("result")

    def disconnect(self):
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None


# ---------------------------------------------------------------------------
# MCP Server
# ---------------------------------------------------------------------------

engine = EngineRPC()
server = Server("kryga-scene")

TOOLS = [
    Tool(
        name="kryga_ping",
        description="Check if the engine is running and responsive",
        inputSchema={"type": "object", "properties": {}, "required": []}
    ),
    Tool(
        name="kryga_scene_list",
        description="List all game objects in the current level (scene tree root)",
        inputSchema={"type": "object", "properties": {}, "required": []}
    ),
    Tool(
        name="kryga_scene_children",
        description="Get children of a game object (its components) or a component (its child components)",
        inputSchema={
            "type": "object",
            "properties": {"id": {"type": "string", "description": "Object or component ID"}},
            "required": ["id"]
        }
    ),
    Tool(
        name="kryga_scene_create",
        description="Create a new empty game object in the scene",
        inputSchema={
            "type": "object",
            "properties": {"name": {"type": "string", "description": "Unique name/ID for the new object"}},
            "required": ["name"]
        }
    ),
    Tool(
        name="kryga_scene_delete",
        description="Delete a game object from the scene",
        inputSchema={
            "type": "object",
            "properties": {"id": {"type": "string", "description": "Game object ID to delete"}},
            "required": ["id"]
        }
    ),
    Tool(
        name="kryga_scene_duplicate",
        description="Duplicate a game object (creates a clone with a generated ID)",
        inputSchema={
            "type": "object",
            "properties": {"id": {"type": "string", "description": "Game object ID to duplicate"}},
            "required": ["id"]
        }
    ),
    Tool(
        name="kryga_select",
        description="Select an object or component (outlines it in viewport)",
        inputSchema={
            "type": "object",
            "properties": {"id": {"type": "string", "description": "Object/component ID to select. Empty string to deselect."}},
            "required": ["id"]
        }
    ),
    Tool(
        name="kryga_transform_get",
        description="Get position, rotation, and scale of an object or component",
        inputSchema={
            "type": "object",
            "properties": {"id": {"type": "string", "description": "Object or component ID"}},
            "required": ["id"]
        }
    ),
    Tool(
        name="kryga_transform_set",
        description="Set position, rotation, and/or scale. Only provided fields are changed.",
        inputSchema={
            "type": "object",
            "properties": {
                "id": {"type": "string", "description": "Object or component ID"},
                "position": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3, "description": "[x, y, z]"},
                "rotation": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3, "description": "[pitch, yaw, roll] in degrees"},
                "scale": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3, "description": "[sx, sy, sz]"},
            },
            "required": ["id"]
        }
    ),
    Tool(
        name="kryga_properties_get",
        description="Get all reflected properties of a game object or component (full inspector payload)",
        inputSchema={
            "type": "object",
            "properties": {"id": {"type": "string", "description": "Object or component ID"}},
            "required": ["id"]
        }
    ),
    Tool(
        name="kryga_properties_set",
        description="Set a property value on an object or component by field name",
        inputSchema={
            "type": "object",
            "properties": {
                "owner_id": {"type": "string", "description": "Owner (game object or component) ID"},
                "name": {"type": "string", "description": "Property field name"},
                "value": {"description": "New value (type must match: number, bool, string, array for vec)"},
            },
            "required": ["owner_id", "name", "value"]
        }
    ),
    Tool(
        name="kryga_component_list_types",
        description="List all available component types that can be added to game objects",
        inputSchema={"type": "object", "properties": {}, "required": []}
    ),
    Tool(
        name="kryga_component_add",
        description="Add a component to a game object",
        inputSchema={
            "type": "object",
            "properties": {
                "object_id": {"type": "string", "description": "Game object ID to add component to"},
                "type_id": {"type": "string", "description": "Component type ID (from component.listTypes)"},
                "name": {"type": "string", "description": "Optional name for the component (defaults to type_id)"},
            },
            "required": ["object_id", "type_id"]
        }
    ),
    Tool(
        name="kryga_visibility_set",
        description="Show or hide an object/component in the viewport",
        inputSchema={
            "type": "object",
            "properties": {
                "id": {"type": "string", "description": "Object or component ID"},
                "visible": {"type": "boolean", "description": "true to show, false to hide"},
            },
            "required": ["id", "visible"]
        }
    ),
    Tool(
        name="kryga_level_list",
        description="List available levels",
        inputSchema={"type": "object", "properties": {}, "required": []}
    ),
    Tool(
        name="kryga_level_load",
        description="Load a level by ID",
        inputSchema={
            "type": "object",
            "properties": {"id": {"type": "string", "description": "Level ID to load"}},
            "required": ["id"]
        }
    ),
    Tool(
        name="kryga_level_save",
        description="Save the current level to disk",
        inputSchema={"type": "object", "properties": {}, "required": []}
    ),
    Tool(
        name="kryga_engine_mode",
        description="Get or set the engine mode (edit/play)",
        inputSchema={
            "type": "object",
            "properties": {
                "mode": {"type": "string", "enum": ["edit", "play"], "description": "Mode to set. Omit to just read current mode."},
            },
            "required": []
        }
    ),
    Tool(
        name="kryga_batch_duplicate",
        description="Duplicate a game object multiple times, optionally placing each clone at a given position. Returns list of created IDs.",
        inputSchema={
            "type": "object",
            "properties": {
                "id": {"type": "string", "description": "Source game object ID to duplicate"},
                "count": {"type": "integer", "description": "Number of duplicates to create"},
                "positions": {"type": "array", "items": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3}, "description": "Optional list of [x,y,z] positions, one per clone. If fewer than count, remaining clones keep source position."},
            },
            "required": ["id", "count"]
        }
    ),
]

# Map tool names to RPC methods and param transforms
TOOL_RPC_MAP = {
    "kryga_ping": ("ping", lambda p: {}),
    "kryga_scene_list": ("scene.getRoot", lambda p: {}),
    "kryga_scene_children": ("scene.getChildren", lambda p: {"id": p["id"]}),
    "kryga_scene_create": ("scene.create", lambda p: {"name": p["name"]}),
    "kryga_scene_delete": ("scene.delete", lambda p: {"id": p["id"]}),
    "kryga_scene_duplicate": ("scene.duplicate", lambda p: {"id": p["id"]}),
    "kryga_select": ("selection.set", lambda p: {"id": p["id"]}),
    "kryga_transform_get": ("transform.get", lambda p: {"id": p["id"]}),
    "kryga_transform_set": ("transform.set", lambda p: {
        "id": p["id"],
        **{k: p[k] for k in ("position", "rotation", "scale") if k in p}
    }),
    "kryga_properties_get": ("properties.get", lambda p: {"id": p["id"]}),
    "kryga_properties_set": ("properties.set", lambda p: {
        "owner_id": p["owner_id"], "name": p["name"], "value": p["value"]
    }),
    "kryga_component_list_types": ("component.listTypes", lambda p: {}),
    "kryga_component_add": ("component.add", lambda p: {
        "object_id": p["object_id"],
        "type_id": p["type_id"],
        **({"name": p["name"]} if "name" in p else {})
    }),
    "kryga_visibility_set": ("visibility.set", lambda p: {
        "id": p["id"], "visible": p["visible"]
    }),
    "kryga_level_list": ("level.list", lambda p: {}),
    "kryga_level_load": ("level.load", lambda p: {"id": p["id"]}),
    "kryga_level_save": ("level.save", lambda p: {}),
}


@server.list_tools()
async def list_tools():
    return TOOLS


@server.call_tool()
async def call_tool(name: str, arguments: dict) -> list[TextContent]:
    # Special case: engine mode (get vs set)
    if name == "kryga_engine_mode":
        try:
            if "mode" in arguments:
                result = engine.call("engine.setMode", {"mode": arguments["mode"]})
            else:
                result = engine.call("engine.getMode")
            return [TextContent(type="text", text=json.dumps(result, indent=2))]
        except Exception as e:
            return [TextContent(type="text", text=f"Error: {e}")]

    # Special case: batch duplicate
    if name == "kryga_batch_duplicate":
        try:
            src_id = arguments["id"]
            count = arguments["count"]
            positions = arguments.get("positions", [])
            created = []
            for i in range(count):
                dup = engine.call("scene.duplicate", {"id": src_id})
                new_id = dup.get("id") if isinstance(dup, dict) else None
                if not new_id:
                    created.append({"index": i, "error": "duplicate returned no id", "raw": dup})
                    continue
                if i < len(positions):
                    engine.call("transform.set", {"id": new_id, "position": positions[i]})
                    created.append({"id": new_id, "position": positions[i]})
                else:
                    created.append({"id": new_id})
            return [TextContent(type="text", text=json.dumps({"created": len(created), "objects": created}, indent=2))]
        except Exception as e:
            return [TextContent(type="text", text=f"Error after {len(created)} duplicates: {e}")]

    if name not in TOOL_RPC_MAP:
        return [TextContent(type="text", text=f"Unknown tool: {name}")]

    method, param_fn = TOOL_RPC_MAP[name]
    try:
        params = param_fn(arguments)
        result = engine.call(method, params)
        return [TextContent(type="text", text=json.dumps(result, indent=2))]
    except Exception as e:
        return [TextContent(type="text", text=f"Error: {e}")]


async def main():
    async with stdio_server() as (read_stream, write_stream):
        init_options = server.create_initialization_options()
        await server.run(read_stream, write_stream, init_options)


if __name__ == "__main__":
    import asyncio
    asyncio.run(main())
