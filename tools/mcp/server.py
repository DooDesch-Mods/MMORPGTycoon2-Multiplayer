"""MMORPG Tycoon 2 local MCP server using the official Python SDK."""
from __future__ import annotations

import argparse
from functools import partial
import json
import logging
import math
from pathlib import Path
import re
import struct
import sys

import anyio
from mcp import types
from mcp.server import Server
from mcp.server.lowlevel import NotificationOptions
from mcp.server.stdio import stdio_server

from runtime import Runtime, RuntimeErrorDetail, OPS, MUTATIONS
import placement_ui_ops
import move_ui_ops
import object_ui_ops
import live_ops
import monster_zone_probe_ops
import native_tool_prepare_ops
import terrain_height_ops
import scenery_gesture_ops
import lobby_ops
import loan_ops
import release_ops
import developer_staff_ops
import bug_policy_ops
import bug_fix_ui_ops
import subscriber_lifecycle_read
import subscriber_capture_ops
import subscriber_append_ops
import marketing_probe_ops
import node_observer_ops
import research_ui_ops

LOG = logging.getLogger("mt2_mcp")
EMPTY = {"type": "object", "properties": {}, "additionalProperties": False}


def object_schema(properties: dict, required=()) -> dict:
    return {"type": "object", "properties": properties, "required": list(required), "additionalProperties": False}


BASE = {
    "list_instances": ("List owned local test manifests and exact process identities.", EMPTY, True),
    "bind_runtime": ("Bind exactly one owned live test instance by its instance.json manifest.",
                     object_schema({"manifest_path": {"type": "string"}}, ("manifest_path",)), False),
    "runtime_status": ("Read status and capability evidence from the bound runtime.", EMPTY, True),
    "read_subscriber_lifecycle": ("Read fixed Subscriber/Toon lifecycle and Region fields, optionally adjusted play hours, current VIP-to-Subscriber links and Campaign/BoostWindow bindings, from the exact owned paused process. No game callback or write.",
        object_schema({"serialized_id": {"type": "integer", "minimum": 0, "maximum": 2147483647}, "include_scenario_vips": {"type": "boolean", "default": False}, "include_campaign_boosts": {"type": "boolean", "default": False}}, ("serialized_id",)), True),
    "read_logs": ("Read a bounded tail of an allowlisted diagnostic log in the bound runtime.",
                  object_schema({"filename": {"type": "string", "enum": ["runtime.log", "mt2mp.log", "diagnostics.log"]},
                                 "max_bytes": {"type": "integer", "minimum": 1, "maximum": 65536}}), True),
    "observe_pending": ("Observe a pending mailbox response and release only after acknowledgement or confirmed exit.", EMPTY, False),
    "recover_pending": ("Recover an owned prior-run lock only after exact previous manifest identity and confirmed process exit.",
                        object_schema({"previous_manifest_path": {"type": "string"}}, ("previous_manifest_path",)), False),
}
TOKEN = {"type": "string", "minLength": 1, "maxLength": 128, "pattern": "^[A-Za-z0-9_.-]+$"}
FLOAT32_MAX = float.fromhex("0x1.fffffep+127")
REGION = object_schema({key: {"type": "number", "minimum": -FLOAT32_MAX, "maximum": FLOAT32_MAX}
                        for key in ("x", "z")}, ("x", "z"))
CAMERA = object_schema({key: {"type": "number", "minimum": -FLOAT32_MAX, "maximum": FLOAT32_MAX}
                        for key in ("x", "y", "z")}, ("x", "y", "z", "distance"))
CAMERA["properties"]["distance"] = {"type": "number", "exclusiveMinimum": 0, "maximum": FLOAT32_MAX}
PLACEMENT = object_schema({"type": {"type": "string", "minLength": 1, "maxLength": 256},
                           "variant": {"type": "string", "minLength": 1, "maxLength": 256},
                           "x": {"type": "number"}, "z": {"type": "number"}}, ("type", "variant", "x", "z"))
POSITIVE_UINT64 = {"type": "integer", "minimum": 1, "maximum": 18446744073709551615}
TIME_KEYS = ("SPACE", "F1", "F2", "F3", "F4")
TIME_TOKEN_PATTERN = r"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"
TIME_TOKEN = {"type": "string", "minLength": 36, "maxLength": 36, "pattern": TIME_TOKEN_PATTERN}
SUBSCRIBER_ID = {"type": "integer", "minimum": 0, "maximum": 2147483647}
NEEDS = {"type": "array", "minItems": 7, "maxItems": 7,
         "items": {"type": "number", "minimum": -100, "maximum": 100}}
from building_keys import KEY_PATTERN, valid_building_key
SHARED_BUILDING_KEY_PATTERN = "^" + KEY_PATTERN + "$"
NETWORK_KEY_PATTERN = r"(?:action:[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}:network:(?:node|pipe)|[0-9a-f]{64}:world:network:(?:node|pipe):[1-9][0-9]{0,9})"
def valid_network_key(value):
    return isinstance(value,str) and re.fullmatch(NETWORK_KEY_PATTERN,value) is not None and (value.startswith("action:") or int(value.rsplit(":",1)[1])<=2147483647)
REMOVE_UI_KEY_PATTERN = "^(?:" + KEY_PATTERN + "|" + NETWORK_KEY_PATTERN + ")$"
FINITE_FLOAT = {"type": "number", "minimum": -FLOAT32_MAX, "maximum": FLOAT32_MAX}
BUILDING_STATE = object_schema({
    "type": {"type": "string", "minLength": 1, "maxLength": 256},
    "variant": {"type": "string", "minLength": 1, "maxLength": 256},
    "x": FINITE_FLOAT, "y": FINITE_FLOAT, "z": FINITE_FLOAT,
    "rotation": {"type": "array", "minItems": 4, "maxItems": 4, "items": FINITE_FLOAT},
    "scale": {"type": "array", "minItems": 3, "maxItems": 3, "items": FINITE_FLOAT},
    "alive": {"type": "boolean", "const": True}}, ("type", "variant", "x", "y", "z", "rotation", "scale", "alive"))


def validate_dungeon_config_ui(arguments, *, read=False):
    fields = {"token"} if read else {"token", "key", "desired", "read_only"}
    if not isinstance(arguments, dict) or set(arguments) != fields:
        raise RuntimeErrorDetail("invalid_args", "Dungeon config UI requires exact token and original control fields")
    if not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None:
        raise RuntimeErrorDetail("invalid_args", "Dungeon config UI token invalid")
    if read:
        return
    if not isinstance(arguments["key"], str) or re.fullmatch(SHARED_BUILDING_KEY_PATTERN, arguments["key"]) is None or type(arguments["desired"]) is not int or not 1 <= arguments["desired"] <= 10 or type(arguments["read_only"]) is not bool:
        raise RuntimeErrorDetail("invalid_args", "Dungeon config UI requires a canonical exterior key, desired1..10 and boolean read_only")


def validate_npc_property(arguments):
    if not isinstance(arguments, dict) or set(arguments) != {"token", "key", "kind", "name", "online", "read_only"}:
        raise RuntimeErrorDetail("invalid_args", "NPC property UI requires exact token/key/kind/name/online/read_only")
    kind, name, online = (arguments[key] for key in ("kind", "name", "online"))
    if not isinstance(arguments["token"], str) or not re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) or not isinstance(arguments["key"], str) or not 1 <= len(arguments["key"]) <= 256 or "\0" in arguments["key"] or type(kind) is not int or kind not in (1, 2, 3, 4) or not isinstance(name, str) or type(online) is not int or online not in (0, 1, 2) or type(arguments["read_only"]) is not bool:
        raise RuntimeErrorDetail("invalid_args", "NPC property UI token/key/intent invalid")
    if (kind == 1 and (not 1 <= len(name.encode("utf-8")) <= 256 or "\0" in name or online != 0)) or (kind != 1 and name != "") or (kind in (3, 4) and online != 0):
        raise RuntimeErrorDetail("invalid_args", "NPC property UI intent contains unrelated fields")


def saved_monster_key(value):
    if not isinstance(value, str):
        return False
    match = re.fullmatch(r"[0-9a-f]{64}:world:monster:(?:([1-9][0-9]{0,9})|inline:region\{(0|[1-9][0-9]{0,9})\}/monsterManager/monster\[(0|[1-9][0-9]{0,9})\])", value)
    return match is not None and all(int(n) <= 2147483647 for n in match.groups() if n is not None)


def npc_target_schema(move=False, monster=False):
    properties = {"token": TIME_TOKEN, "target_action_uuid": TIME_TOKEN,
                  "record_key": {"type": "string", "minLength": 1, "maxLength": 256}}
    if monster:
        properties["family"] = {"type": "string", "const": "monster"}
        properties["target_action_uuid"] = {"type": "string", "minLength": 36, "maxLength": 256}
    required = ["token"]
    if move:
        properties["position"] = {"type": "array", "minItems": 3, "maxItems": 3,
                                  "items": {"type": "number", "minimum": -1000000, "maximum": 1000000}}
        required.append("position")
    schema = object_schema(properties, required)
    schema["oneOf"] = [{"required": ["target_action_uuid"]}, {"required": ["record_key"]}]
    return schema


def validate_character_rename_ui(arguments):
    if not isinstance(arguments, dict) or set(arguments) not in ({"token", "kind", "id", "name", "read_only"}, {"token", "kind", "id", "name", "read_only", "open"}):
        raise RuntimeErrorDetail("invalid_args", "Character Rename UI requires exact token/kind/id/name/read_only")
    if not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None:
        raise RuntimeErrorDetail("invalid_args", "Character Rename UI token must be a canonical UUID")
    if type(arguments["kind"]) is not int or not -1 <= arguments["kind"] <= 4 or type(arguments["id"]) is not int or not -1 <= arguments["id"] < 1000000:
        raise RuntimeErrorDetail("invalid_args", "Character Rename UI target is outside the native domain")
    if type(arguments["read_only"]) is not bool or not isinstance(arguments["name"], str) or len(arguments["name"]) > 20 or "\0" in arguments["name"]:
        raise RuntimeErrorDetail("invalid_args", "Character Rename UI requires Boolean read_only and a native name of at most 20 characters")
    if arguments["read_only"] and arguments["name"] or not arguments["read_only"] and (arguments["kind"] < 0 or arguments["id"] < 0):
        raise RuntimeErrorDetail("invalid_args", "Character Rename inspection needs an empty name; mutation needs an exact current target")
    if "open" in arguments and (type(arguments["open"]) is not bool or arguments["open"] and (arguments["kind"] < 0 or arguments["id"] < 0)):
        raise RuntimeErrorDetail("invalid_args", "Character original class-card open needs Boolean open and an exact current target")


def validate_character_edit_ui(arguments):
    required = {"token", "operation", "kind", "id"}
    if not isinstance(arguments, dict) or not required <= arguments.keys():
        raise RuntimeErrorDetail("invalid_args", "Character offered UI requires token/operation/kind/id")
    fields = {
        "ShowCostume": set(), "CostumeState": set(), "ToggleWeapon": {"category", "expected_enabled"},
        "ShowAbility": set(), "AbilityState": {"slot"}, "ApplyAbility": {"slot"},
        "ShowAbilityPage": {"slot", "page"}, "EditAbilityText": {"slot", "control", "value"},
        "EditAbilityChoice": {"slot", "control", "value"}, "ToggleAbilityControl": {"slot", "control", "expected_enabled"},
        "ActivateAbilityControl": {"slot", "control"},
    }
    operation = arguments["operation"]
    if not isinstance(operation, str) or operation not in fields:
        raise RuntimeErrorDetail("invalid_args", "Character offered UI operation is unsupported")
    expected = required | fields[operation]
    if operation == "ShowAbility" and "slot" in arguments:
        expected.add("slot")
    if operation == "EditAbilityText" and "commit" in arguments:
        expected.add("commit")
    if set(arguments) != expected or not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None:
        raise RuntimeErrorDetail("invalid_args", "Character offered UI requires exact fields and a canonical fresh token")
    if type(arguments["kind"]) is not int or not 0 <= arguments["kind"] <= 4 or type(arguments["id"]) is not int or not 0 <= arguments["id"] < 1000000:
        raise RuntimeErrorDetail("invalid_args", "Character offered UI requires an exact native definition")
    if "slot" in expected and (type(arguments["slot"]) is not int or not 0 <= arguments["slot"] < 10000):
        raise RuntimeErrorDetail("invalid_args", "Ability offered UI requires an actual canonical slot")
    for key in {"control", "page", "value", "category"} & expected:
        value = arguments[key]
        limit = 4096 if key == "value" else 128 if key == "category" else 512
        try:
            valid = isinstance(value, str) and len(value.encode("utf-8")) <= limit and "\0" not in value and (key == "value" or bool(value))
        except UnicodeError:
            valid = False
        if not valid or (key == "category" and any(c in value for c in "\r\n")):
            raise RuntimeErrorDetail("invalid_args", "Character offered UI text is invalid: " + key)
    if "expected_enabled" in expected and type(arguments["expected_enabled"]) is not bool:
        raise RuntimeErrorDetail("invalid_args", "Character offered UI expected_enabled must be Boolean")
    if "commit" in expected and type(arguments["commit"]) is not bool:
        raise RuntimeErrorDetail("invalid_args", "Ability original text commit flag must be Boolean")


def validate_character_edit_read(arguments):
    if not isinstance(arguments, dict) or set(arguments) != {"token"} or not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None:
        raise RuntimeErrorDetail("invalid_args", "Character offered input read requires one canonical retained token")


def validate_character_rename_read(arguments):
    if (not isinstance(arguments, dict) or set(arguments) not in (set(), {"action_uuid"})
            or "action_uuid" in arguments and (not isinstance(arguments["action_uuid"], str)
            or re.fullmatch(TIME_TOKEN_PATTERN, arguments["action_uuid"]) is None)):
        raise RuntimeErrorDetail("invalid_args", "Character read requires empty args or one canonical action_uuid")


def validate_npc_target(arguments, move=False, monster=False):
    if not isinstance(arguments, dict):
        raise RuntimeErrorDetail("invalid_args", "NPC UI requires an object with exactly one target")
    if "family" in arguments:
        if not monster or arguments["family"] != "monster" or "record_key" in arguments:
            raise RuntimeErrorDetail("invalid_args", "Monster UI requires family monster and one confirmed canonical target")
    targets = set(arguments) & {"target_action_uuid", "record_key"}
    required = {"token"} | targets | ({"position"} if move else set()) | ({"family"} if "family" in arguments else set())
    if len(targets) != 1 or set(arguments) != required:
        raise RuntimeErrorDetail("invalid_args", "NPC UI requires token and exactly one placement UUID or saved record key")
    if not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None:
        raise RuntimeErrorDetail("invalid_args", "NPC UI requires a canonical token UUID")
    target = next(iter(targets))
    value = arguments[target]
    if target == "target_action_uuid":
        saved = arguments.get("family") == "monster" and saved_monster_key(value)
        if not isinstance(value, str) or (re.fullmatch(TIME_TOKEN_PATTERN, value) is None and not saved):
            raise RuntimeErrorDetail("invalid_args", "NPC UI placement UUID invalid")
    elif not isinstance(value, str) or not 1 <= len(value) <= 256 or "\0" in value:
        raise RuntimeErrorDetail("invalid_args", "NPC UI saved record key must contain 1 through 256 characters")
    # The native shared snapshot key parser validates the saved record grammar and current world binding.
    if move:
        point = arguments["position"]
        if not isinstance(point, list) or len(point) != 3 or any(type(value) not in (int, float) or abs(value) > 1000000 or not math.isfinite(value) for value in point):
            raise RuntimeErrorDetail("invalid_args", "NPC move UI requires finite bounded position")


def validate_npc_region(arguments):
    if (not isinstance(arguments, dict) or set(arguments) not in ({"region_id"}, {"region_id", "quest_action_uuid"})
            or type(arguments["region_id"]) is not int or not 0 <= arguments["region_id"] <= 2147483647):
        raise RuntimeErrorDetail("invalid_args", "read_npc_region requires region_id and optional quest_action_uuid")
    if "quest_action_uuid" in arguments and (not isinstance(arguments["quest_action_uuid"], str)
            or re.fullmatch(TIME_TOKEN_PATTERN, arguments["quest_action_uuid"]) is None):
        raise RuntimeErrorDetail("invalid_args", "read_npc_region quest_action_uuid requires canonical UUID")


def validate_npc_quest_edit(arguments):
    if not isinstance(arguments, dict) or set(arguments) != {"token", "owner_key", "region_id", "remove_slot", "edit"}:
        raise RuntimeErrorDetail("invalid_args", "Quest edit UI requires token, owner_key, region_id, current remove_slot and edit")
    validate_npc_quest_list({key: value for key, value in arguments.items() if key != "edit"})
    value = arguments["edit"]
    if arguments["remove_slot"] < 0 or not isinstance(value, dict) or set(value) != {"kind", "title", "kills", "xp", "gold", "defaults"}:
        raise RuntimeErrorDetail("invalid_args", "Quest edit requires an existing slot and exact editor fields")
    if type(value["kind"]) is not int or value["kind"] not in (1, 2, 3) or not isinstance(value["title"], str) or len(value["title"].encode("utf-8")) > 1024 or "\0" in value["title"] or type(value["defaults"]) is not bool:
        raise RuntimeErrorDetail("invalid_args", "Quest edit kind/title/defaults outside domain")
    if any(type(value[key]) is not int or not 0 <= value[key] <= limit for key, limit in (("kills", 1000000), ("xp", 2147483647), ("gold", 2147483647))):
        raise RuntimeErrorDetail("invalid_args", "Quest edit integer outside domain")
    if ((value["kind"] == 1 and (not value["title"] or value["kills"] or value["xp"] or value["gold"] or value["defaults"])) or
            (value["kind"] == 2 and (value["title"] or value["xp"] or value["gold"] or value["defaults"])) or
            (value["kind"] == 3 and (value["title"] or value["kills"] or (value["defaults"] and (value["xp"] or value["gold"]))))):
        raise RuntimeErrorDetail("invalid_args", "Quest edit carries unrelated command values")


def validate_npc_quest_list(arguments):
    if not isinstance(arguments, dict) or set(arguments) != {"token", "owner_key", "region_id", "remove_slot"}:
        raise RuntimeErrorDetail("invalid_args", "Quest list UI requires exact token, owner_key, region_id and remove_slot")
    if not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None:
        raise RuntimeErrorDetail("invalid_args", "Quest list UI requires a canonical token UUID")
    if not isinstance(arguments["owner_key"], str) or not 1 <= len(arguments["owner_key"]) <= 256 or "\0" in arguments["owner_key"]:
        raise RuntimeErrorDetail("invalid_args", "Quest list canonical owner key invalid")
    if type(arguments["region_id"]) is not int or not 0 <= arguments["region_id"] <= 2147483647 or type(arguments["remove_slot"]) is not int or not -1 <= arguments["remove_slot"] <= 4:
        raise RuntimeErrorDetail("invalid_args", "Quest list region or current slot outside domain")


def validate_npc_quest_target(arguments):
    required = {"token", "owner_key", "sequence_id", "region_id", "destination_region", "destination_key", "family"}
    if not isinstance(arguments, dict) or set(arguments) != required:
        raise RuntimeErrorDetail("invalid_args", "Quest target UI requires exact existing owner slot and destination fields")
    if not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None:
        raise RuntimeErrorDetail("invalid_args", "Quest target UI requires a canonical token UUID")
    for key in ("owner_key", "destination_key"):
        if not isinstance(arguments[key], str) or not 1 <= len(arguments[key]) <= 256 or "\0" in arguments[key]:
            raise RuntimeErrorDetail("invalid_args", "Quest target canonical keys require 1 through 256 characters")
    if any(type(arguments[key]) is not int or not 0 <= arguments[key] <= 2147483647 for key in ("region_id", "destination_region")) or type(arguments["sequence_id"]) is not int or not 0 <= arguments["sequence_id"] <= 4095:
        raise RuntimeErrorDetail("invalid_args", "Quest target region or existing slot outside domain")
    if arguments["family"] not in ("building", "zone", "monster", "npc"):
        raise RuntimeErrorDetail("invalid_args", "Quest target family must be building, zone, monster or npc")
    if arguments["family"] == "monster" and not saved_monster_key(arguments["destination_key"]) and (
            not arguments["destination_key"].startswith("action:") or
            re.fullmatch(TIME_TOKEN_PATTERN, arguments["destination_key"][len("action:"):]) is None):
        raise RuntimeErrorDetail("invalid_args", "Quest direct Monster destination requires a confirmed canonical Monster key")


OP_SCHEMAS = {
    "enable_ui_build": EMPTY,
    "request_building_place": object_schema({"action_uuid": TIME_TOKEN, "building": BUILDING_STATE}, ("action_uuid", "building")),
    "request_building_move": object_schema({"action_uuid": TIME_TOKEN, "record_key": {"type":"string","pattern":SHARED_BUILDING_KEY_PATTERN}, "building": BUILDING_STATE}, ("action_uuid", "record_key", "building")),
    "enable_ui_move": EMPTY,
    "enable_ui_network": EMPTY,
    "native_network_pipe": object_schema({"token": TIME_TOKEN, "id": SUBSCRIBER_ID, "start_node": SUBSCRIBER_ID, "end_node": {"type": "integer", "minimum": -1, "maximum": 2147483647}, "graph_sha256": {"type": "string", "pattern": "^[a-f0-9]{64}$"}, "end": {"type": "array", "minItems": 3, "maxItems": 3, "items": FINITE_FLOAT}}, ("token", "id", "start_node", "end_node", "end", "graph_sha256")),
    "read_network_placement": object_schema({"id": SUBSCRIBER_ID}, ("id",)),
    "native_network_node": object_schema({"token": TIME_TOKEN, "id": SUBSCRIBER_ID, "type": {"type": "string", "minLength": 1, "maxLength": 128}, "key": {"type": "string", "minLength": 1, "maxLength": 128}, "position": {"type": "array", "minItems": 3, "maxItems": 3, "items": {"type": "number"}}}, ("token", "id", "type", "key", "position")),
    "enable_ui_path": EMPTY,
    "native_path_delete": object_schema({"token": TIME_TOKEN, "id": SUBSCRIBER_ID, "path_index": {"type": "integer", "minimum": 0, "maximum": 999999}}, ("token", "id", "path_index")),
    "read_path": object_schema({"id": SUBSCRIBER_ID}, ("id",)),
    "read_world_path": object_schema({"id": SUBSCRIBER_ID}, ("id",)),
    "native_world_path": object_schema({"token": TIME_TOKEN, "id": SUBSCRIBER_ID, "kind": {"type":"string","enum":["wall","pass"]}, "start": {"type":"array","minItems":3,"maxItems":3,"items":{"type":"number"}}, "end": {"type":"array","minItems":3,"maxItems":3,"items":{"type":"number"}}}, ("token","id","kind","start","end")),
    "native_path_segment": object_schema({"token": TIME_TOKEN, "id": SUBSCRIBER_ID, "start": {"type": "array", "minItems": 3, "maxItems": 3, "items": {"type": "number"}}, "end": {"type": "array", "minItems": 3, "maxItems": 3, "items": {"type": "number"}}}, ("token", "id", "start", "end")),
    "enable_ui_terrain": EMPTY,
    "read_terrain": object_schema({"id": SUBSCRIBER_ID}, ("id",)),
    "native_terrain_stroke": object_schema({"token": TIME_TOKEN, "id": SUBSCRIBER_ID, "style": {"type": "integer", "minimum": 0, "maximum": 3}, "points": {"type": "array", "minItems": 1, "maxItems": 1024, "items": {"type": "array", "minItems": 3, "maxItems": 3, "items": {"type": "number"}}}}, ("token", "id", "style", "points")),
    "enable_ui_region": EMPTY,
    "read_region_preset": object_schema({"region_id": SUBSCRIBER_ID}, ("region_id",)),
    "read_npc_region": object_schema({"region_id": SUBSCRIBER_ID, "quest_action_uuid": TIME_TOKEN}, ("region_id",)),
    "native_npc_tool_ui": object_schema({"token": TIME_TOKEN, "region_id": SUBSCRIBER_ID,
        "tool": {"type": "string", "enum": ["Guard", "ClassTrainer", "QuestGiver"]},
        "phase": {"type": "string", "enum": ["open", "select"]},
        "character_slot": {"type": "integer", "minimum": -1, "maximum": 2147483647},
        "character_type": {"type": "integer", "minimum": -1, "maximum": 2147483647}},
        ("token", "region_id", "tool", "phase", "character_slot", "character_type")),
    "native_npc_place_probe": object_schema({"token": TIME_TOKEN, "region_id": SUBSCRIBER_ID,
        "tool": {"type": "string", "enum": ["Guard", "ClassTrainer", "QuestGiver"]},
        "character_slot": {"type": "integer", "minimum": 0, "maximum": 2147483647},
        "character_type": {"type": "integer", "minimum": 0, "maximum": 2147483647},
        "position": {"type": "array", "minItems": 3, "maxItems": 3,
            "items": {"type": "number"}}},
        ("token", "region_id", "tool", "character_slot", "character_type", "position")),
    "native_npc_place_ui": object_schema({"token": TIME_TOKEN, "region_id": SUBSCRIBER_ID,
        "tool": {"type": "string", "enum": ["Guard", "ClassTrainer", "QuestGiver", "item_elitemonster"]},
        "character_slot": {"type": "integer", "minimum": 0, "maximum": 2147483647},
        "character_type": {"type": "integer", "minimum": 0, "maximum": 2147483647},
        "level": {"type": "integer", "minimum": 1, "maximum": 2147483647},
        "position": {"type": "array", "minItems": 3, "maxItems": 3,
            "items": {"type": "number"}}},
        ("token", "region_id", "tool", "character_slot", "character_type", "position")),
    "native_character_rename_ui": object_schema({"token": TIME_TOKEN, "kind": {"type": "integer", "minimum": -1, "maximum": 4},
        "id": {"type": "integer", "minimum": -1, "maximum": 999999}, "name": {"type": "string", "maxLength": 20},
        "read_only": {"type": "boolean"}, "open": {"type": "boolean"}}, ("token", "kind", "id", "name", "read_only")),
    "read_character_rename": object_schema({"action_uuid": TIME_TOKEN}),
    "native_character_edit_ui": object_schema({"token": TIME_TOKEN,
        "operation": {"type": "string", "enum": ["ShowCostume", "CostumeState", "ToggleWeapon", "ShowAbility", "AbilityState", "ShowAbilityPage", "EditAbilityText", "EditAbilityChoice", "ToggleAbilityControl", "ActivateAbilityControl", "ApplyAbility"]},
        "kind": {"type": "integer", "minimum": 0, "maximum": 4}, "id": {"type": "integer", "minimum": 0, "maximum": 999999},
        "slot": {"type": "integer", "minimum": 0, "maximum": 9999},
        "control": {"type": "string", "minLength": 1, "maxLength": 512}, "page": {"type": "string", "minLength": 1, "maxLength": 512},
        "value": {"type": "string", "maxLength": 4096}, "commit": {"type": "boolean"},
        "category": {"type": "string", "minLength": 1, "maxLength": 128}, "expected_enabled": {"type": "boolean"}},
        ("token", "operation", "kind", "id")),
    "read_character_edit_ui": object_schema({"token": TIME_TOKEN}),
    "native_npc_remove_ui": npc_target_schema(monster=True),
    "native_dungeon_config_ui": object_schema({"token": TIME_TOKEN, "key": {"type": "string", "pattern": SHARED_BUILDING_KEY_PATTERN}, "desired": {"type": "integer", "minimum": 1, "maximum": 10}, "read_only": {"type": "boolean"}}),
    "read_dungeon_config_ui": object_schema({"token": TIME_TOKEN}),
    "native_npc_property_ui": object_schema({"token": TIME_TOKEN, "key": {"type": "string", "minLength": 1, "maxLength": 256},
        "kind": {"type": "integer", "minimum": 1, "maximum": 4}, "name": {"type": "string", "maxLength": 256},
        "online": {"type": "integer", "minimum": 0, "maximum": 2}, "read_only": {"type": "boolean"}},
        ("token", "key", "kind", "name", "online", "read_only")),
    "native_npc_move_ui": npc_target_schema(move=True, monster=True),
    "native_npc_quest_edit_ui": object_schema({"token": TIME_TOKEN, "owner_key": {"type": "string", "minLength": 1, "maxLength": 256}, "region_id": {"type": "integer", "minimum": 0, "maximum": 2147483647}, "remove_slot": {"type": "integer", "minimum": 0, "maximum": 4}, "edit": object_schema({"kind": {"type": "integer", "minimum": 1, "maximum": 3}, "title": {"type": "string", "maxLength": 1024}, "kills": {"type": "integer", "minimum": 0, "maximum": 1000000}, "xp": {"type": "integer", "minimum": 0, "maximum": 2147483647}, "gold": {"type": "integer", "minimum": 0, "maximum": 2147483647}, "defaults": {"type": "boolean"}})}),
    "native_npc_quest_list_ui": object_schema({"token": TIME_TOKEN, "owner_key": {"type": "string", "minLength": 1, "maxLength": 256}, "region_id": {"type": "integer", "minimum": 0, "maximum": 2147483647}, "remove_slot": {"type": "integer", "minimum": -1, "maximum": 4}}),
    "native_npc_quest_target_ui": object_schema({"token": TIME_TOKEN, "owner_key": {"type": "string", "minLength": 1, "maxLength": 256},
        "sequence_id": {"type": "integer", "minimum": 0, "maximum": 4095}, "region_id": SUBSCRIBER_ID, "destination_region": SUBSCRIBER_ID,
        "destination_key": {"type": "string", "minLength": 1, "maxLength": 256}, "family": {"type": "string", "enum": ["building", "zone", "monster", "npc"]}},
        ("token", "owner_key", "sequence_id", "region_id", "destination_region", "destination_key", "family")),
    "read_prices": EMPTY,
    "native_price_ui": object_schema({"diagnostic_uuid": TIME_TOKEN,
        "purchase_fee": {"type": "integer", "minimum": 0, "maximum": 2147483647},
        "subscription_fee": {"type": "integer", "minimum": 0, "maximum": 2147483647}},
        ("diagnostic_uuid", "purchase_fee", "subscription_fee")),
    "native_region_styles_ui": object_schema({"diagnostic_uuid": TIME_TOKEN, "region_id": SUBSCRIBER_ID, "styles": {"type": "array", "minItems": 4, "maxItems": 4, "items": {"type": "string", "minLength": 1, "maxLength": 256}}}, ("diagnostic_uuid", "region_id", "styles")),
    "native_region_preset_ui": object_schema({"diagnostic_uuid": TIME_TOKEN, "region_id": SUBSCRIBER_ID, "preset": {"type": "string", "minLength": 1, "maxLength": 256}}, ("diagnostic_uuid", "region_id", "preset")),
    "native_region_property_ui": object_schema({"diagnostic_uuid": TIME_TOKEN, "region_id": SUBSCRIBER_ID,
        "kind": {"type": "string", "enum": ["name", "type", "purchase", "levels", "sale", "tint", "light", "ambient", "sky", "particles", "particle_color"]},
        "name": {"type": "string", "maxLength": 256}, "type": {"type": "integer", "minimum": -1, "maximum": 1},
        "hit": {"type": "array", "minItems": 3, "maxItems": 3, "items": FINITE_FLOAT},
        "levels": {"type": "array", "minItems": 2, "maxItems": 2, "items": {"type": "integer", "minimum": 1, "maximum": 214748354}},
        "appearance_value": {"type": "array", "minItems": 16, "maxItems": 16, "items": FINITE_FLOAT}},
        ("diagnostic_uuid", "region_id", "kind", "name", "type", "hit")),
    "start_typed_actions": EMPTY,
    "enable_ui_remove": EMPTY,
    "probe_remove_ui": object_schema({"diagnostic_uuid": TIME_TOKEN,"snapshot_uuid": TIME_TOKEN,
        "record_key": {"type": "string", "pattern": REMOVE_UI_KEY_PATTERN}}, ("diagnostic_uuid","snapshot_uuid","record_key")),
    "request_snapshot_remove": object_schema({"action_uuid": TIME_TOKEN,
        "record_key": {"type": "string", "pattern": SHARED_BUILDING_KEY_PATTERN}}, ("action_uuid", "record_key")),
    "retire_snapshot_remove": object_schema({"token": TIME_TOKEN}, ("token",)),
    "read_remove_bindings": EMPTY,
    "probe_remove_snapshot_building": object_schema({"token": TIME_TOKEN, "snapshot_id": TIME_TOKEN,
        "record_key": {"type": "string", "minLength": 1, "maxLength": 4096},
        "expected_world_epoch": POSITIVE_UINT64, "expected_world_generation": POSITIVE_UINT64},
        ("token", "snapshot_id", "record_key", "expected_world_epoch", "expected_world_generation")),
    "read_native_free": EMPTY,
    "probe_native_holder": object_schema({"token": TIME_TOKEN, "kind": {"type": "string", "enum": ["prop", "building"]},
                                         "expected_world_epoch": POSITIVE_UINT64, "expected_world_generation": POSITIVE_UINT64},
                                        ("token", "kind", "expected_world_epoch", "expected_world_generation")),
    "status": EMPTY,
    "read_building_history_ui": EMPTY,
    "native_building_history_ui": object_schema({"direction": {"type":"string","enum":["undo","redo"]},
        "snapshot_uuid": TIME_TOKEN, "expected_host_revision": {"type":"integer","minimum":0},
        "target_action_uuid": TIME_TOKEN},
        ("direction","snapshot_uuid","expected_host_revision","target_action_uuid")),
    "read_chat": EMPTY,
    "send_chat": object_schema({"text": {"type": "string", "minLength": 1, "maxLength": 240}}, ("text",)),
    "inspect_engine": EMPTY,
    "resync_world": EMPTY,
    "read_network_state": EMPTY,
    "read_tutorial": EMPTY,
    "read_tutorial_development": EMPTY,
    "native_research_ui": object_schema({"token": TIME_TOKEN, "native_origin_uuid": TIME_TOKEN,
        "operation": {"type":"string","enum":list(research_ui_ops.COMMANDS)},
        "tech_key": {"type":"string","minLength":1,"maxLength":200},
        "world_epoch": {"type":"integer","minimum":1}, "world_generation": {"type":"integer","minimum":1},
        "expected_development": {"type":"object"}}, tuple(sorted(research_ui_ops.FIELDS))),
    "tutorial_startingpoint": object_schema({"token": TIME_TOKEN, "native_origin_uuid": TIME_TOKEN,
        "operation": {"type":"string","enum":["begin","show","complete","commit","client_show","client_commit","client_research_show","client_research","client_research_state","client_staff_show","client_staff_state","client_staff_one","staff_activity_state","client_bridges_show","client_bridges_state","client_bridges_start","client_bridges_staff_show","client_bridges_staff_state","client_bridges_staff_one","client_bridges_commit","client_bridges_commit_state","client_startingpoint_commit_state"]},
        "world_epoch": {"type":"integer","minimum":1}, "world_generation": {"type":"integer","minimum":1},
        "expected_development": {"type":"object"}},
        ("token","native_origin_uuid","operation","world_epoch","world_generation","expected_development")),
    "read_vip_alert_owners": EMPTY,
    "read_tutorial_progress": EMPTY,
    "read_tutorial_movement": EMPTY,
    "read_tutorial_movement_focus": EMPTY,
    "tutorial_movement_focus": object_schema({"native_origin_uuid": TIME_TOKEN}, ("native_origin_uuid",)),
    "tutorial_movement": object_schema({"acceptance_token": TIME_TOKEN, "native_origin_uuid": TIME_TOKEN}, ("acceptance_token", "native_origin_uuid")),
    "tutorial_movement_complete": object_schema({"acceptance_token": TIME_TOKEN, "native_origin_uuid": TIME_TOKEN}, ("acceptance_token", "native_origin_uuid")),
    "read_tutorial_camera": EMPTY,
    "tutorial_camera_complete": object_schema({"acceptance_token": TIME_TOKEN, "native_origin_uuid": TIME_TOKEN, "current_step_index": {"type": "integer", "enum": [8, 10, 12]}}, ("acceptance_token", "native_origin_uuid", "current_step_index")),
    "tutorial_welcome_ok": object_schema({"token": TIME_TOKEN, "snapshot_uuid": TIME_TOKEN,
        "world_epoch": POSITIVE_UINT64, "world_generation": POSITIVE_UINT64,
        "current_step_index": {"type": "integer", "const": 4}},
        ("token", "snapshot_uuid", "world_epoch", "world_generation", "current_step_index")),
    "tutorial_leveltwo_message_ok": object_schema({"token": TIME_TOKEN, "snapshot_uuid": TIME_TOKEN,
        "world_epoch": POSITIVE_UINT64, "world_generation": POSITIVE_UINT64,
        "current_step_index": {"type": "integer", "enum": [2, 4, 6, 9, 11, 13, 14]}},
        ("token", "snapshot_uuid", "world_epoch", "world_generation", "current_step_index")),
    "tutorial_initial_message_ok": object_schema({"token": TIME_TOKEN, "snapshot_uuid": TIME_TOKEN,
        "world_epoch": POSITIVE_UINT64, "world_generation": POSITIVE_UINT64,
        "current_step_index": {"type": "integer", "enum": [4, 15, 19, 22, 32, 38, 43, 48, 53, 66, 79, 103]}},
        ("token", "snapshot_uuid", "world_epoch", "world_generation", "current_step_index")),
    "tutorial_finish_ok": object_schema({"token": TIME_TOKEN, "snapshot_uuid": TIME_TOKEN,
        "world_epoch": POSITIVE_UINT64, "world_generation": POSITIVE_UINT64,
        "current_step_index": {"type": "integer", "const": 103}},
        ("token", "snapshot_uuid", "world_epoch", "world_generation", "current_step_index")),
    "tutorial_network_tab": object_schema({"token": TIME_TOKEN, "snapshot_uuid": TIME_TOKEN,
        "world_epoch": POSITIVE_UINT64, "world_generation": POSITIVE_UINT64,
        "current_step_index": {"type": "integer", "const": 56}},
        ("token", "snapshot_uuid", "world_epoch", "world_generation", "current_step_index")),
    "read_time_ui": EMPTY,
    "read_time_acceptance": EMPTY,
    "native_time_key": object_schema({"key": {"type": "string", "enum": list(TIME_KEYS)},
                                     "expected_epoch": POSITIVE_UINT64, "expected_revision": POSITIVE_UINT64,
                                     "expected_request_sequence": POSITIVE_UINT64, "acceptance_token": TIME_TOKEN},
                                    ("key", "expected_epoch", "expected_revision", "expected_request_sequence", "acceptance_token")),
    "read_time_input": object_schema({"acceptance_token": TIME_TOKEN}, ("acceptance_token",)),
    "native_time_button": object_schema({"acceptance_token": TIME_TOKEN,"snapshot_uuid": TIME_TOKEN,
        "scale":{"type":"number","enum":[0,1,3,9]},"expected_epoch":POSITIVE_UINT64,"expected_world_epoch":POSITIVE_UINT64,
        "expected_generation":POSITIVE_UINT64,"expected_revision":POSITIVE_UINT64,"expected_request_sequence":POSITIVE_UINT64},
        ("acceptance_token","snapshot_uuid","scale","expected_epoch","expected_world_epoch","expected_generation","expected_revision","expected_request_sequence")),
    "read_time_mouse": object_schema({"acceptance_token":TIME_TOKEN},("acceptance_token",)),
    "read_building_catalog": EMPTY,
    "read_subscriber_candidates": EMPTY,
    "read_subscriber_needs": object_schema({"serialized_id":{"type":"integer","minimum":0,"maximum":2147483647}},("serialized_id",)),
    "read_subscriber_weapon": object_schema({"serialized_id":{"type":"integer","minimum":0,"maximum":2147483647},"include_motion":{"type":"boolean"},"include_animation":{"type":"boolean"}},("serialized_id",)),
    "arm_subscriber_needs": object_schema({"epoch": POSITIVE_UINT64, "serialized_id": SUBSCRIBER_ID, "expected_initial": NEEDS},
                                         ("epoch", "serialized_id", "expected_initial")),
    "apply_subscriber_needs": object_schema({"epoch": POSITIVE_UINT64, "serialized_id": SUBSCRIBER_ID, "revision": POSITIVE_UINT64,
                                             "expected_before": NEEDS, "host_needs": NEEDS},
                                            ("epoch", "serialized_id", "revision", "expected_before", "host_needs")),
    "disarm_subscriber_needs": object_schema({"epoch": POSITIVE_UINT64}, ("epoch",)),
    "capture_snapshot": EMPTY,
    "capture_local_snapshot": EMPTY,
    "capture_frame": EMPTY,
    "create_test_world": EMPTY,
    "exit_game": EMPTY,
    "dismiss_crash_report": EMPTY,
    "probe_building_placement": PLACEMENT,
    "place_test_building": PLACEMENT,
    "purchase_test_region": REGION,
    "focus_test_camera": CAMERA,
    "set_time_scale": object_schema({"scale": {"type": "number", "minimum": 0}}, ("scale",)),
    "load_snapshot": object_schema({"snapshot_id": TOKEN}, ("snapshot_id",)),
    "execute_action": object_schema({"action_id": TOKEN, "parameters": {"type": "object"}}, ("action_id", "parameters")),
}
OP_DESCRIPTIONS = {
    "read_character_rename": "Empty args list actual live base class candidates. One action_uuid returns only retained command-boundary Character state, complete Definition/Actor/Release/Quest/ArtStore proof hashes and full Ledger, without a new native census. Original offered menu/card controls determine candidate eligibility.",
    "read_character_edit_ui": "Return the retained original-input receipt and ordered action UUID for one Character edit token, without a new census or replay.",
    "native_character_edit_ui": "Use actual offered Character controls in an owned ordered pair. ShowAbility without slot opens the original Abilities page and lists only visible eligible icons mapped to canonical slots, UID, name, level and Boolean positive_effects; an empty offer is a successful empty array. Supplying one actual offered slot opens its genuine icon. AbilityState lists the complete live/draft records and shown inputs. EditAbilityText uses the actual character limit and commits a real text control by default; commit false intentionally leaves its original focused input uncommitted for preservation acceptance. EditAbilityChoice accepts only its current catalogue values, ToggleAbilityControl uses the real checkbox and ShowAbilityPage selects only an enabled stock page. ActivateAbilityControl invokes another shown editor button; prefab saving is excluded. ApplyAbility invokes the shown original Apply once and links the captured complete draft to its ordered UUID. ShowCostume opens the original Design/class card and enabled Costume/Settings tabs. CostumeState lists only shown eligible category checkboxes. ToggleWeapon requires one actual category and its expected current enabled state; native checkbox Press/Release captures one ordered intent. Inspect the retained token and both peer results. Never replay an attempted token.",
    "native_dungeon_config_ui": "Select the actual Dungeon exterior and enabled original Instances tab. Normal UI update completes waiting_for_tab; the retained job then validates visible Configure and reciprocal owner, invokes original OpenInstancesConfig, Spinner +/- and Accept once. read_only inspects controls without accepting. Observe the token with read_dungeon_config_ui; never replay it.",
    "read_dungeon_config_ui": "Read one retained Dungeon config UI token and its captured action, original-once status and canonical before/after Config, full Ledger and RNG evidence. Readback never replays input.",
    "native_npc_property_ui": "Select one exact current NPC, verify its offered visible Name/Online/Level control, and invoke its original Header/InfoWindow callback once. read_only selects and inspects the control without committing a property. Observe ui_npc.ui_jobs and complete ordered property receipts. Never replay an attempted token.",
    "enable_ui_build": "Enable normal building intent capture and original native building execution on the bound game thread.",
    "request_building_place": "Submit a complete building request through the existing Host-ordered controller; admission is not a native completion receipt.",
    "request_building_move": "Submit one full-pose Move request for an existing shared building through Host authority. Observe the exact UUID and native inverse Undo; never replay Unknown.",
    "enable_ui_move": "Install normal single-building Move capture after starting the connected Controller.",
    "enable_ui_network": "Capture the genuine NetworkNode Press through the existing Controller; requires enabled Region readback.",
    "read_network_placement": "Read complete network nodes, pipes, connections and canonical state without local pointers or UIDs.",
    "native_network_pipe": "Activate the original Cable ItemButton and complete one original Node-to-Node/Junction span through the shared action Controller. Uses region-local node indices under an exact graph digest.",
    "native_network_node": "Activate the actual enabled Node ItemButton, Hover and Press once; observe ui_network.ui_jobs and never replay a token.",
    "enable_ui_path": "Capture the original Road purchase intent on the existing Controller. Requires enabled Region readback.",
    "native_path_delete": "Select an existing native Road, show its real PathInfoWindow and invoke its Destroy UICommand once. Observe ui_path.ui_jobs; never replay an attempted token.",
    "read_path": "Read original Road topology, local UIDs, canonical graph digest and native side effects without replay.",
    "read_world_path": "Read the complete canonical mixed Path graph, original Records, effects and native Pass border candidates.",
    "native_world_path": "Select an existing original Wall or Pass ItemButton and call native input once inside OriginalGo; preserve the token and never replay a failed attempt.",
    "native_path_segment": "Activate the actual Road ItemButton and original Hover/Press callbacks once for two new endpoints. Observe ui_path.ui_jobs; never replay an attempted token.",
    "enable_ui_terrain": "Capture original TerrainBrush strokes on the existing Controller. Requires Region readback already enabled.",
    "read_terrain": "Read actual terrain contours, scenery, RNG, release points, bug digest and retained UI job receipts.",
    "native_terrain_stroke": "Activate the actual Terrain ItemButton and send original MouseOver/Press/Drag/Release once. Observe the token under ui_terrain.ui_jobs until the existing Host controller confirms; never replay an attempted token.",
    "enable_ui_region": "Install genuine RegionInfoWindow PresetApply intent capture on the existing Controller.",
    "read_region_preset": "Read actual native region preset, four styles, RNG words and UID-free scenery digest.",
    "read_npc_region": "Read actual NPC rows and bounded Quest samples in one saved region. Optional quest_action_uuid returns only cached command-boundary Quest records/tasks/Ledger for that exact retained operation, without another native scan. Local UIDs are diagnostic only.",
    "native_npc_tool_ui": "Open an actual Guard or Trainer item button and list ThemeWindow CharacterTypeIDs, or select one original ThemeList entry. Uses a fresh token for each UI attempt; open requires CharacterTypeID [-1,-1].",
    "native_npc_place_probe": "In an isolated MPTest world only, call original NPC MouseOver and Release once after ThemeWindow selection. Measure actual NPC rows, RNG, full Ledger and Undo. This mutates the test world; never replay an attempted token.",
    "native_npc_place_ui": "On a connected peer, capture one Host-ordered placement through the original Cursor Release dispatcher. Guard/ClassTrainer/QuestGiver use the existing active NPC UI. item_elitemonster additionally requires level and performs original Region/tool/selection/level preparation. Inspect ui_npc or ui_monster UI job and Controller ACK. Never replay an attempted token.",
    "native_character_rename_ui": "Inspect the actual offered Character Rename control, or queue original BeginRename and native text input inside OriginalGo. Set open true to start with the class collection hidden, press the actual Design ItemButton if needed, select its offered unlocked pcs menu row, then click the actual class card. Complete gameplay/ArtStore/RNG proof surrounds collection construction and editor opening. Otherwise a live unlocked Character pane is required. Inspect ui_character_rename.ui_jobs and complete peer effects; never replay an attempted token.",
    "native_npc_remove_ui": "Select one NPC from its confirmed placement action and invoke original DestroyNPC. With family=monster, select one confirmed direct Monster birth and invoke original MonsterInfoWindow Destroy. Capture one Host-ordered removal; inspect ui_npc and Controller ACK before another action. Never replay an attempted or unknown token.",
    "native_npc_quest_edit_ui": "Run original QuestSelector SetTitle(kind1), SetKills(kind2), or SetRewards(kind3) for an existing current slot. Inspect ui_npc_quest_list and ACK3; never replay an attempted/unknown token.",
    "native_npc_quest_list_ui": "Run original NPCInfoWindow AddQuest for remove_slot=-1 or QuestSelector Destroy for an existing current slot0..4. Inspect ui_npc_quest_list and ACK3; never replay an attempted or unknown token.",
    "native_npc_quest_target_ui": "Edit one existing NPC Quest slot through original QuestSelector SetTarget and Cursor Press. Resolve a Building, MonsterZone, canonical NPC or confirmed direct Monster destination in the current world. Inspect ui_npc_quest and Controller ACK; never replay an attempted or unknown token. Does not add or delete a Quest.",
    "native_npc_move_ui": "With family=monster, use a confirmed direct Monster birth and original MonsterInfoWindow Move, then normal Update/Purchase capture. Move one NPC from its confirmed placement action through the original NPCInfoWindow Move and cursor placement. Capture one Host-ordered move; inspect npc_action and Controller ACK before another action. Never replay an attempted or unknown token.",
    "read_prices": "Read the actual bound Business purchase and subscription fees plus the full current Ledger.",
    "native_price_ui": "Set the actual Overview PropertyPane input values and invoke original UpdateSaleValues once. Observe ui_price.ui_attempts and the Controller ACK; never replay an attempted diagnostic UUID.",
    "native_region_styles_ui": "Select the actual RegionWidget, set four original Terrain combo values and dispatch TerrainApply once. Observe ui_region.last_action_uuid for Host confirmation; never replay the diagnostic UUID.",
    "native_region_preset_ui": "Select the native RegionWidget, set the original preset selector and dispatch PresetApply once. Observe ui_region.last_action_uuid for Host confirmation; never replay the diagnostic UUID after failure.",
    "native_region_property_ui": "Trigger one original region Rename, Buy/Lift Press/Release, or actual SetRegionLevel toolbar/range/Press/Drag/Release UI sequence. The hook captures one Host-ordered intent; read ui_region.property_operations for confirmed readback. Never replay the diagnostic UUID.",
    "start_typed_actions": "Bind the current actual native world to the authoritative action controller. Start once on every required peer before submitting actions.",
    "enable_ui_remove": "Install the current build's normal Remove UI capture after the actual Controller is started. One installation attempt per owned process; retain failed receipts.",
    "probe_remove_ui": "Invoke the original typed InfoWindow Destroy command once with a genuine native record. Network input is queued for actual OriginalGo; observe its retained action UUID for command completion. Never replay the diagnostic UUID after failure.",
    "request_snapshot_remove": "Submit one typed snapshot building removal to Host authority and wait for all native application and retirement acknowledgements. A queued request is not completion; preserve its UUID and never replay Unknown.",
    "retire_snapshot_remove": "Release the actual private holders of one confirmed snapshot removal through its retained token and native quiescence journal. Preserve partial receipts; never replay an unknown attempt.",
    "read_remove_bindings": "Read current native snapshot building keys and local UID diagnostics for one owned world.",
    "probe_remove_snapshot_building": "Remove one current native snapshot building through its guarded private engine adapter. This diagnostic does not replicate a client request. Preserve the token; never replay an unknown attempt.",
    "read_native_free": "Read native Free installation status and retained holder probe receipts on the game thread.",
    "probe_native_holder": "Create and free one private genuine null engine holder after exact world guards. Retain the token receipt; never replay a failed or unknown attempt.",
    "status": "Request current game-thread state and observe the matching acknowledgement.",
    "read_chat": "Read bounded chat entries and actual recipient acknowledgement masks from the bound session.",
    "send_chat": "Queue one bounded in-game chat message in the bound session. The immediate receipt confirms queue admission only; use read_chat for recipient acknowledgement.",
    "native_building_history_ui": "Invoke one guarded original native Undo or Redo entry on the bound Mainthread. The receipt proves UI intent admission only; read both peers and never replay Unknown.",
    "read_building_history_ui": "Read native Undo and Redo stack depths, current shared graph targets, hook counters and Controller receipt without mutation.",
    "inspect_engine": "Inspect verified engine metadata on the game thread.",
    "resync_world": "Request a fresh shared snapshot through the existing Host and owned native load path. The receipt confirms queue admission only. Observe game_status.world_reload for completion/failure; never replay a timed-out request.",
    "read_network_state": "Read native network capacity and bounded node samples; observation indices are local and not portable IDs.",
    "read_tutorial": "Read the current native tutorial scenario, indexed step and completion counts without changing progress or exposing process pointers.",
    "read_tutorial_development": "Read the current world's Starting Point project, technology record and inventory membership without changing game state or exposing process pointers.",
    "native_research_ui": "Use the genuine offered Research row and original Start, Staff or Commit control for an actual catalogue Tech key in an owned ordered pair. Read returns the complete generic project proof and actual completion without advancing work. Requires fresh token and exact native Origin/development guards. Never replay an uncertain UI attempt. No completion, cash or unlock setter is exposed.",
    "tutorial_startingpoint": "Fixed development UI proof: client_bridges_commit_state and client_startingpoint_commit_state only read the exact current project completion, deployed inventory and native owner; both support running ordered simulation and never finish work. client_bridges_show/state/start use the actual owned Bridges Research row and original button; client_bridges_staff_show/state/one use its original Staff Pips; client_bridges_commit activates its original enabled Commit button after natural completion. These operations confirm only Client intent submission, not peer completion. No completion setter is offered for Bridges. Existing Starting Point proof: staff_activity_state reads actual owned Developer activities and probes both fixed native localized project labels, with native call receipts and unchanged project/full ledger. It requires an initialized idle joined Host or Client, Clock0 and purchased current project; native temporary-object exceptions are Unknown and require process restart before another probe. client_staff_show opens the native Staff page; client_staff_state only measures Staff/dev/Pips/button readiness; client_staff_one activates the first owned toggle Button once for one worker and confirms only a fresh pending Host intent with unchanged local row and full ledger. Staff requires a purchased current Startingpoint project with desired workers0. client_research_show opens the window and requests its Tech page; client_research_state only reads page/owner/Grid/button readiness; client_research selects the actual catalogue Grid row and activates its enabled Research button once and confirms only a fresh pending Host intent with unchanged local row and full ledger. client_show opens the joined Client Development window; client_commit activates its enabled original Commit button and confirms only a pending Host-ordered intent with unchanged local gameplay. Client operations require initialized idle authoritative Client, Clock0 and exact Origin. Existing Host operations: pay for and begin the owned catalogue project through the original engine routines, show the native Development window, complete the existing project, or activate its real Commit button. Requires an uninitialized or idle authoritative Controller, Clock0 and exact current native development readback. Begin requires an existing unpurchased record and no current project. Keep each token; never replay an unknown effect.",
    "read_vip_alert_owners": "Read actual current HUD alert Window class, AlertList ancestry and ScenarioVip arrival ownership on the bound game thread. Never creates, shows, hides or destroys alerts.",
    "read_tutorial_progress": "Read the joined world's actual tutorial progress display and Client Sequence fence without changing input, completion or game state.",
    "read_tutorial_movement": "Read the fixed local Host movement fixture receipt. Queued input is not confirmed input. Never replay an unresolved attempt.",
    "read_tutorial_movement_focus": "Read actual owned Host SDL keyboard focus and native keyboard-input admission. No input or focus mutation.",
    "tutorial_movement_focus": "Ask SDL once to raise the exact owned local Host window. Read focus and native input admission before the fixed W fixture. No global input or focus APIs.",
    "tutorial_movement_complete": "Hold original W input on the owned Host until WASD completes, at most 15 seconds. Read the same movement receipt for ordered completion and key release. No replay.",
    "read_tutorial_camera": "Read the owned original wheel/RMB input, release and ordered tutorial completion receipt. No replay.",
    "tutorial_camera_complete": "Complete one exact Initial camera prompt with bounded original SDL wheel or RMB/motion input. Never writes camera or tutorial state; releases RMB on failure.",
    "tutorial_movement": "Queue one fixed W down/up pair to the owned local Host window at tutorial step6. Read its receipt for native input, progress and release confirmation. No replay.",
    "tutorial_welcome_ok": "Hide only the current native Host welcome message once through its original window Show path. Requires exact local origin and index4; read tutorial afterward to confirm native progression. Never replay an unknown result.",
    "tutorial_network_tab": "Press only the actual current Host Network ActionBar tab at Initial step56 once. Uses its existing authored command and original Press. Requires exact owned local origin. Confirm view readback, then observe normal tutorial progression; never replay Unknown.",
    "tutorial_leveltwo_message_ok": "Hide only one of the seven fixed native Host LevelTwo messages through its original Show path. Index must match the authored message ID. Requires exact owned local origin; normal Update owns progression. Never replay Unknown.",
    "tutorial_initial_message_ok": "Close one exact authored Initial tutorial popup through original native Show. No cursor or completion writes; inspect ordered progression afterward.",
    "tutorial_finish_ok": "Hide only the current native Host initial_tutorial/allDone message at index103 through its original window Show path. Requires exact local origin; normal Host Update owns completion. Read both tutorials afterward. Never replay an unknown result.",
    "read_time_ui": "Read the four owned native time-button states and authoritative presentation revision from the shared host/client runtime.",
    "read_time_acceptance": "Read owned native time-input guards, bindings, actor states and authoritative receipt counters; this does not prove calendar or economy authority.",
    "native_time_key": "Queue one guarded SPACE or F1-F4 diagnostic input in the owned test world. Preserve its token and inspect its receipt; never replay a partial or unknown input.",
    "read_time_input": "Read the immutable diagnostic input receipt by token, including separate queued, native-consumed, hook and host acknowledgement states.",
    "native_time_button": "Invoke one genuine current HUD ItemButton Activate entry in the owned test world. Preserve its token; native return is not Host confirmation. Never replay after failure or Unknown.",
    "read_time_mouse": "Observe one retained native mouse diagnostic, including exact hook origin, local suppression and Host confirmation.",
    "read_building_catalog": "Count guarded native root building-manager arrays and return at most 32 samples; dungeon scopes remain explicitly unobserved.",
    "read_subscriber_candidates": "Read at most 32 existing logged-in subscriber IDs from the guarded native manager, without guessing consecutive IDs.",
    "read_subscriber_needs": "Read seven native needs and bounded subscriber metadata by serialized int32 ID on the game thread.",
    "arm_subscriber_needs": "Arm a diagnostic write fence for exactly one subscriber in the owned local client and active software epoch.",
    "apply_subscriber_needs": "Apply seven host needs to the selected diagnostic subscriber through guarded native setters, with before and after receipts.",
    "disarm_subscriber_needs": "Disarm the selected diagnostic subscriber write fence for its exact epoch.",
    "capture_snapshot": "Capture an engine snapshot through a verified game-thread capability.",
    "capture_local_snapshot": "Retain a verified native snapshot in the owned runtime without distributing it or loading another peer.",
    "capture_frame": "Capture the native rendered game framebuffer to an owned diagnostic artifact.",
    "create_test_world": "Create the owned MPTestWorld through a verified native game-thread capability.",
    "exit_game": "Request graceful native shutdown of the bound owned test instance.",
    "dismiss_crash_report": "Dismiss the active crash report through native CrashReport::Command Cancel (Don't Upload).",
    "probe_building_placement": "Probe native MPTest diagnostic building placement without purchasing or editing.",
    "place_test_building": "Send a scoped MPTest diagnostic placement request to the host and wait for host result and local replica.",
    "purchase_test_region": "Purchase the first region of the owned MPTest world through native affordability, cash subtraction and ownership services, then read back the result. Diagnostic preparation only.",
    "focus_test_camera": "Focus the owned MPTest diagnostic camera through native Camera.FocusOn and wait for actual camera readback. The native target interpolates.",
    "read_subscriber_weapon": "Read one current joined Subscriber's chosen weapon and actual local Actor/Skeleton/attachment/matrix on the game thread. Does not equip, refresh, or run simulation.",
    "set_time_scale": "Set time scale through a verified game-thread capability.",
    "load_snapshot": "Validate local snapshot size and SHA256, then wait for native world activation through its opaque snapshot identifier.",
    "execute_action": "Execute a named game action through a verified game-thread capability.",
}


OP_SCHEMAS.update(node_observer_ops.SCHEMAS)
OP_DESCRIPTIONS.update(node_observer_ops.DESCRIPTIONS)
OP_SCHEMAS.update(subscriber_capture_ops.SCHEMAS)
OP_SCHEMAS.update(subscriber_append_ops.SCHEMAS)
OP_DESCRIPTIONS.update(subscriber_append_ops.DESCRIPTIONS)
OP_SCHEMAS.update(marketing_probe_ops.SCHEMAS)
OP_DESCRIPTIONS.update(marketing_probe_ops.DESCRIPTIONS)
OP_DESCRIPTIONS.update(subscriber_capture_ops.DESCRIPTIONS)
OP_SCHEMAS.update(loan_ops.SCHEMAS)
OP_SCHEMAS.update(bug_policy_ops.SCHEMAS)
OP_SCHEMAS.update(bug_fix_ui_ops.SCHEMAS)
OP_SCHEMAS.update(release_ops.SCHEMAS)
OP_SCHEMAS.update(developer_staff_ops.SCHEMAS)
OP_DESCRIPTIONS.update(developer_staff_ops.DESCRIPTIONS)
OP_DESCRIPTIONS.update(bug_policy_ops.DESCRIPTIONS)
OP_DESCRIPTIONS.update(bug_fix_ui_ops.DESCRIPTIONS)
OP_DESCRIPTIONS.update(release_ops.DESCRIPTIONS)
OP_DESCRIPTIONS.update(loan_ops.DESCRIPTIONS)
OP_SCHEMAS.update(lobby_ops.SCHEMAS)
OP_DESCRIPTIONS.update(lobby_ops.DESCRIPTIONS)
OP_SCHEMAS.update(placement_ui_ops.SCHEMAS)
OP_SCHEMAS.update(move_ui_ops.SCHEMAS)
OP_SCHEMAS.update(object_ui_ops.SCHEMAS)
OP_SCHEMAS.update(live_ops.SCHEMAS)
OP_DESCRIPTIONS.update(live_ops.DESCRIPTIONS)
OP_DESCRIPTIONS.update(move_ui_ops.DESCRIPTIONS)
OP_DESCRIPTIONS.update(object_ui_ops.DESCRIPTIONS)
OP_SCHEMAS.update(monster_zone_probe_ops.SCHEMAS)
OP_DESCRIPTIONS.update(monster_zone_probe_ops.DESCRIPTIONS)
OP_SCHEMAS.update(native_tool_prepare_ops.SCHEMAS)
OP_SCHEMAS.update(terrain_height_ops.SCHEMAS)
OP_SCHEMAS.update(scenery_gesture_ops.SCHEMAS)
OP_DESCRIPTIONS.update(native_tool_prepare_ops.DESCRIPTIONS)
OP_DESCRIPTIONS.update(terrain_height_ops.DESCRIPTIONS)
OP_DESCRIPTIONS.update(scenery_gesture_ops.DESCRIPTIONS)
OP_DESCRIPTIONS.update(placement_ui_ops.DESCRIPTIONS)


OP_SCHEMAS['native_terrain_tab'] = native_tool_prepare_ops.SCHEMAS['native_scenery_tab']
OP_SCHEMAS['read_terrain_tab'] = native_tool_prepare_ops.SCHEMAS['read_scenery_tab']
OP_SCHEMAS['native_periodic_countdown'] = EMPTY
OP_SCHEMAS['read_toon_motion_trace'] = EMPTY
OP_SCHEMAS['read_ordered_world'] = EMPTY
OP_DESCRIPTIONS['read_ordered_world'] = 'Read the current ordered world and retained native execution receipt without submitting input.'
OP_DESCRIPTIONS['native_terrain_tab'] = 'Select the original Terrain tab once for the exact current world; retain the token and observe tab_ready before input.'
OP_DESCRIPTIONS['read_terrain_tab'] = 'Read the retained Terrain tab and layout acknowledgement without repeating input.'
OP_DESCRIPTIONS['native_periodic_countdown'] = 'Open the original periodic countdown once on a fresh idle owned ordered Client; rejects earlier delegation and active saves.'
OP_DESCRIPTIONS['read_toon_motion_trace'] = 'Read the retained Toon motion diagnostic in an owned local-test profile without input.'

def tools_for(runtime: Runtime) -> list[types.Tool]:
    tools = [types.Tool(name=name, description=description, inputSchema=schema,
                        annotations=types.ToolAnnotations(readOnlyHint=read_only, destructiveHint=False,
                                                         idempotentHint=read_only, openWorldHint=False))
             for name, (description, schema, read_only) in BASE.items()]
    try:
        available = runtime.available()
    except RuntimeErrorDetail as exc:
        LOG.warning("Capability listing degraded code=%s cause=%s; runtime tools withheld", exc.code, exc)
        available = set()
    for op in sorted(available):
        tools.append(types.Tool(name=f"game_{op}", description=OP_DESCRIPTIONS[op], inputSchema=OP_SCHEMAS[op],
                                annotations=types.ToolAnnotations(readOnlyHint=op in {"read_character_edit_ui", "read_native_gm_policy_ui", "read_native_rules_feature_events", "read_native_rules_numeric_ui", "read_character_rename", "read_dungeon_config_ui", "read_native_rules_ui", "read_ordered_world", "read_terrain_tab", "read_toon_motion_trace", "read_native_service_sync_ui", "read_native_campaign_ui", "read_native_loan_ui", "read_native_gm_ui", "read_release_planner_ui", "read_vip_alert_owners", "read_native_bug_fix_ui", "read_subscriber_weapon", "read_bug_c_policy", "read_native_developer_retirement", "read_native_developers", "read_npc_region", "read_prices", "read_chat", "read_node_observer", "read_native_release", "read_native_loans", "read_native_lobby", "status", "inspect_engine", "probe_building_placement", "read_subscriber_needs", "read_network_state", "read_tutorial", "read_tutorial_development", "read_tutorial_progress", "read_tutorial_movement", "read_tutorial_movement_focus", "read_subscriber_candidates", "read_time_ui", "read_building_catalog", "read_time_acceptance", "read_time_input", "read_time_mouse", "read_native_free", "read_remove_bindings", "read_build_ui", "read_building_history_ui", "read_move_ui", "read_object_ui", "read_cancel_tool"},
                                                                 destructiveHint=op in MUTATIONS,
                                                                 idempotentHint=False, openWorldHint=False)))
    return tools


def dispatch(runtime: Runtime, name: str, arguments: dict) -> dict:
    if name == "list_instances":
        return runtime.list_instances()
    if name == "bind_runtime":
        return runtime.bind(arguments["manifest_path"])
    if name == "runtime_status":
        return runtime.status()
    if name == "read_subscriber_lifecycle":
        return subscriber_lifecycle_read.read(runtime, arguments)
    if name == "read_logs":
        return runtime.read_logs(**arguments)
    if name == "observe_pending":
        return runtime.observe_pending()
    if name == "recover_pending":
        return runtime.recover_pending(arguments["previous_manifest_path"])
    if name.startswith("game_") and name[5:] in OPS:
        op = name[5:]
        if op == "resync_world" and (not isinstance(arguments, dict) or arguments):
            raise RuntimeErrorDetail("invalid_args", "resync_world requires empty object arguments")
        if op == "read_building_history_ui" and (not isinstance(arguments, dict) or arguments):
            raise RuntimeErrorDetail("invalid_args", "read_building_history_ui requires empty object arguments")
        if op == "native_building_history_ui":
            expected = {"direction", "snapshot_uuid", "expected_host_revision", "target_action_uuid"}
            if not isinstance(arguments, dict) or set(arguments) != expected or arguments["direction"] not in ("undo", "redo"):
                raise RuntimeErrorDetail("invalid_args", "building history UI requires exact direction and guards")
            if any(not isinstance(arguments[key], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments[key]) is None for key in ("snapshot_uuid", "target_action_uuid")) or type(arguments["expected_host_revision"]) is not int or not 0 <= arguments["expected_host_revision"] <= 18446744073709551615:
                raise RuntimeErrorDetail("invalid_args", "building history UI snapshot, target or revision invalid")
        if op in {"read_vip_alert_owners", "read_tutorial", "read_tutorial_development", "read_tutorial_progress", "read_tutorial_movement"} and (not isinstance(arguments, dict) or arguments):
            raise RuntimeErrorDetail("invalid_args", op + " requires empty object arguments")
        if op == "read_tutorial_camera" and (not isinstance(arguments, dict) or arguments):
            raise RuntimeErrorDetail("invalid_args", "tutorial camera read requires empty arguments")
        if op == "tutorial_camera_complete":
            if not isinstance(arguments, dict) or set(arguments) != {"acceptance_token", "native_origin_uuid", "current_step_index"}:
                raise RuntimeErrorDetail("invalid_args", "tutorial camera requires exact token/origin/index")
            if type(arguments["current_step_index"]) is not int or arguments["current_step_index"] not in (8, 10, 12):
                raise RuntimeErrorDetail("invalid_args", "unsupported exact camera tutorial step")
            if any(not isinstance(arguments[key], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments[key]) is None for key in ("acceptance_token", "native_origin_uuid")):
                raise RuntimeErrorDetail("invalid_args", "tutorial camera requires lowercase UUIDs")
        if op in {"tutorial_movement", "tutorial_movement_complete"}:
            if not isinstance(arguments, dict) or set(arguments) != {"acceptance_token", "native_origin_uuid"}:
                raise RuntimeErrorDetail("invalid_args", "tutorial movement requires exact token/origin")
            if any(not isinstance(value, str) or re.fullmatch(TIME_TOKEN_PATTERN, value) is None for value in arguments.values()):
                raise RuntimeErrorDetail("invalid_args", "tutorial movement requires lowercase UUIDs")
        if op == "native_research_ui":
            try:
                research_ui_ops.validate(arguments)
            except ValueError as error:
                raise RuntimeErrorDetail("invalid_args", str(error)) from error
        if op == "tutorial_startingpoint":
            required={"token","native_origin_uuid","operation","world_epoch","world_generation","expected_development"}
            if not isinstance(arguments,dict) or set(arguments)!=required or arguments["operation"] not in ("begin","show","complete","commit","client_show","client_commit","client_research_show","client_research","client_research_state","client_staff_show","client_staff_state","client_staff_one","staff_activity_state","client_bridges_show","client_bridges_state","client_bridges_start","client_bridges_staff_show","client_bridges_staff_state","client_bridges_staff_one","client_bridges_commit","client_bridges_commit_state","client_startingpoint_commit_state"):
                raise RuntimeErrorDetail("invalid_args", "startingpoint fixture requires exact fixed operation and guards")
            if any(not isinstance(arguments[key],str) or re.fullmatch(TIME_TOKEN_PATTERN,arguments[key]) is None for key in ("token","native_origin_uuid")):
                raise RuntimeErrorDetail("invalid_args", "startingpoint fixture requires canonical token and native origin UUIDs")
            if any(type(arguments[key]) is not int or not 1<=arguments[key]<=18446744073709551615 for key in ("world_epoch","world_generation")) or not isinstance(arguments["expected_development"],dict) or not arguments["expected_development"]:
                raise RuntimeErrorDetail("invalid_args", "startingpoint fixture requires positive local origin and actual development object")
        if op == "read_tutorial_movement_focus" and (not isinstance(arguments, dict) or arguments):
            raise RuntimeErrorDetail("invalid_args", "tutorial movement focus read requires empty arguments")
        if op == "tutorial_movement_focus":
            if not isinstance(arguments, dict) or set(arguments) != {"native_origin_uuid"} or not isinstance(arguments["native_origin_uuid"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["native_origin_uuid"]) is None:
                raise RuntimeErrorDetail("invalid_args", "tutorial movement focus requires exact lowercase native origin UUID")
        if op in {"tutorial_welcome_ok", "tutorial_finish_ok", "tutorial_network_tab", "tutorial_leveltwo_message_ok", "tutorial_initial_message_ok"}:
            expected_indices = (4, 15, 19, 22, 32, 38, 43, 48, 53, 66, 79, 103) if op == "tutorial_initial_message_ok" else (2, 4, 6, 9, 11, 13, 14) if op == "tutorial_leveltwo_message_ok" else (4,) if op == "tutorial_welcome_ok" else (103,) if op == "tutorial_finish_ok" else (56,)
            required = {"token", "snapshot_uuid", "world_epoch", "world_generation", "current_step_index"}
            if not isinstance(arguments, dict) or set(arguments) != required:
                raise RuntimeErrorDetail("invalid_args", "tutorial welcome requires exact token/origin/index fields")
            if any(not isinstance(arguments[key], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments[key]) is None
                   for key in ("token", "snapshot_uuid")):
                raise RuntimeErrorDetail("invalid_args", "tutorial welcome token and local snapshot require canonical UUIDs")
            if any(type(arguments[key]) is not int or not 1 <= arguments[key] <= 18446744073709551615
                   for key in ("world_epoch", "world_generation")) or type(arguments["current_step_index"]) is not int or arguments["current_step_index"] not in expected_indices:
                raise RuntimeErrorDetail("invalid_args", "tutorial close requires positive local origin and index" + str(expected_indices))
        if op == "read_chat" and (not isinstance(arguments, dict) or arguments):
            raise RuntimeErrorDetail("invalid_args", "read_chat requires empty object arguments")
        if op == "send_chat":
            if not isinstance(arguments, dict) or set(arguments) != {"text"} or not isinstance(arguments["text"], str):
                raise RuntimeErrorDetail("invalid_args", "send_chat requires exactly one text string")
            text = arguments["text"]
            try:
                byte_count = len(text.encode("utf-8"))
            except UnicodeEncodeError as exc:
                raise RuntimeErrorDetail("invalid_args", "send_chat text must be valid UTF-8") from exc
            if not 1 <= byte_count <= 240 or any(ord(char) < 32 or 127 <= ord(char) <= 159 for char in text):
                raise RuntimeErrorDetail("invalid_args", "send_chat text must contain 1 to 240 UTF-8 bytes without control characters")
        if op in node_observer_ops.SCHEMAS:
            node_observer_ops.validate(op, arguments)
        if op in subscriber_capture_ops.SCHEMAS:
            subscriber_capture_ops.validate(op, arguments)
        if op in marketing_probe_ops.SCHEMAS:
            marketing_probe_ops.validate(op, arguments)
        if op in subscriber_append_ops.SCHEMAS:
            subscriber_append_ops.validate(op, arguments)
        if op in bug_fix_ui_ops.SCHEMAS:
            bug_fix_ui_ops.validate(op, arguments)
            return runtime.request(op, arguments)
        if op in bug_policy_ops.SCHEMAS:
            bug_policy_ops.validate(op, arguments)
        if op in release_ops.SCHEMAS:
            release_ops.validate(op, arguments)
        if op in developer_staff_ops.SCHEMAS:
            developer_staff_ops.validate(op, arguments)
        if op in loan_ops.SCHEMAS:
            loan_ops.validate(op, arguments)
        if op in lobby_ops.SCHEMAS:
            lobby_ops.validate(op, arguments)
        if op in live_ops.SCHEMAS:
            live_ops.validate(op, arguments)
        if op in terrain_height_ops.SCHEMAS:
            terrain_height_ops.validate(op,arguments)
        if op in scenery_gesture_ops.SCHEMAS:
            scenery_gesture_ops.validate(op,arguments)
        if op in monster_zone_probe_ops.SCHEMAS:
            monster_zone_probe_ops.validate(op,arguments)
        if op in native_tool_prepare_ops.SCHEMAS:
            native_tool_prepare_ops.validate(op, arguments)
        if op in move_ui_ops.SCHEMAS:
            move_ui_ops.validate(op, arguments)
        if op in object_ui_ops.SCHEMAS:
            object_ui_ops.validate(op, arguments)
        if op in placement_ui_ops.SCHEMAS:
            placement_ui_ops.validate(op, arguments)
        if op in {"start_typed_actions", "enable_ui_remove", "enable_ui_build", "enable_ui_move", "enable_ui_region", "enable_ui_terrain", "enable_ui_path", "enable_ui_network"} and (not isinstance(arguments, dict) or arguments):
            raise RuntimeErrorDetail("invalid_args", "typed action start requires empty arguments")
        if op == "native_network_pipe":
            expected = {"token", "id", "start_node", "end_node", "end", "graph_sha256"}
            if not isinstance(arguments, dict) or set(arguments) != expected:
                raise RuntimeErrorDetail("invalid_args", "network Pipe requires exact token, region, endpoint selectors, end and graph digest")
            if any(type(arguments[k]) is not int or not (0 if k != "end_node" else -1) <= arguments[k] <= 2147483647 for k in ("id", "start_node", "end_node")) or arguments["start_node"] == arguments["end_node"]:
                raise RuntimeErrorDetail("invalid_args", "network Pipe endpoint selectors invalid")
            if not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None or not isinstance(arguments["graph_sha256"], str) or re.fullmatch(r"[a-f0-9]{64}", arguments["graph_sha256"]) is None:
                raise RuntimeErrorDetail("invalid_args", "network Pipe token/digest invalid")
            point = arguments["end"]
            if not isinstance(point, list) or len(point) != 3 or any(type(v) not in (int, float) or not math.isfinite(v) or abs(v) > 3.4028234663852886e38 for v in point):
                raise RuntimeErrorDetail("invalid_args", "network Pipe end requires finite float32 coordinates")
        if op in {"read_network_placement", "native_network_node"}:
            expected = {"id"} if op == "read_network_placement" else {"token", "id", "type", "key", "position"}
            if not isinstance(arguments, dict) or set(arguments) != expected or type(arguments["id"]) is not int or not 0 <= arguments["id"] <= 2147483647:
                raise RuntimeErrorDetail("invalid_args", "network requires exact nonnegative int32 ID and native UI fields")
            if op == "native_network_node":
                if not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None:
                    raise RuntimeErrorDetail("invalid_args", "network native UI requires canonical UUID token")
                if any(not isinstance(arguments[k], str) or not 1 <= len(arguments[k]) <= 128 or chr(0) in arguments[k] for k in ("type", "key")):
                    raise RuntimeErrorDetail("invalid_args", "network requires canonical native type/key strings")
                point = arguments["position"]
                if not isinstance(point, list) or len(point) != 3 or any(type(v) not in (int, float) or not math.isfinite(v) or abs(v) > 3.4028234663852886e38 for v in point):
                    raise RuntimeErrorDetail("invalid_args", "network native UI requires finite float32 position")
        if op in {"read_path", "native_path_segment", "native_path_delete", "read_world_path", "native_world_path"}:
            expected = {"id"} if op in {"read_path","read_world_path"} else {"token","id","kind","start","end"} if op == "native_world_path" else {"token", "id", "path_index"} if op == "native_path_delete" else {"token", "id", "start", "end"}
            if not isinstance(arguments, dict) or set(arguments) != expected or type(arguments["id"]) is not int or not 0 <= arguments["id"] <= 2147483647:
                raise RuntimeErrorDetail("invalid_args", "path requires exact nonnegative int32 ID and native UI fields")
            if op == "native_path_delete":
                if not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None or type(arguments["path_index"]) is not int or not 0 <= arguments["path_index"] < 1000000:
                    raise RuntimeErrorDetail("invalid_args", "path delete native UI requires canonical UUID token and bounded path index")
            if op in {"native_path_segment","native_world_path"}:
                if op == "native_world_path" and arguments["kind"] not in ("wall","pass"):
                    raise RuntimeErrorDetail("invalid_args", "World Path kind requires wall or pass")
                if not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None:
                    raise RuntimeErrorDetail("invalid_args", "path native UI requires canonical UUID token")
                points = [arguments["start"], arguments["end"]]
                if any(not isinstance(p, list) or len(p) != 3 or any(type(v) not in (int, float) or not math.isfinite(v) or abs(v) > 3.4028234663852886e38 for v in p) for p in points) or ((points[0] == points[1]) != (op == "native_world_path" and arguments["kind"] == "pass")):
                    raise RuntimeErrorDetail("invalid_args", "path native UI requires two distinct finite float32 triples")
        if op in {"read_terrain", "native_terrain_stroke"}:
            expected = {"id"} if op == "read_terrain" else {"token", "id", "style", "points"}
            if not isinstance(arguments, dict) or set(arguments) != expected or type(arguments["id"]) is not int or not 0 <= arguments["id"] <= 2147483647:
                raise RuntimeErrorDetail("invalid_args", "terrain requires exact nonnegative int32 ID and native UI fields")
            if op == "native_terrain_stroke":
                points = arguments["points"]
                if not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None or type(arguments["style"]) is not int or not 0 <= arguments["style"] <= 3 or not isinstance(points, list) or not 1 <= len(points) <= 1024:
                    raise RuntimeErrorDetail("invalid_args", "terrain native UI token/style/point count invalid")
                if any(not isinstance(p, list) or len(p) != 3 or any(type(v) not in (int, float) or not math.isfinite(v) or abs(v) > 3.4028234663852886e38 for v in p) for p in points):
                    raise RuntimeErrorDetail("invalid_args", "terrain native UI points require finite float32 triples")
        if op == "native_region_styles_ui":
            if not isinstance(arguments, dict) or set(arguments) != {"region_id", "diagnostic_uuid", "styles"} or type(arguments["region_id"]) is not int or not 0 <= arguments["region_id"] <= 2147483647:
                raise RuntimeErrorDetail("invalid_args", "region styles requires exact nonnegative int32 ID, UUID and styles")
            if not isinstance(arguments["diagnostic_uuid"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["diagnostic_uuid"]) is None or not isinstance(arguments["styles"], list) or len(arguments["styles"]) != 4 or any(not isinstance(v, str) or not 1 <= len(v.encode("utf-8")) <= 256 or "\0" in v for v in arguments["styles"]):
                raise RuntimeErrorDetail("invalid_args", "region styles requires canonical UUID and four bounded strings")
        if op == "read_npc_region":
            validate_npc_region(arguments)
        if op == "native_npc_tool_ui":
            required = {"token", "region_id", "tool", "phase", "character_slot", "character_type"}
            if not isinstance(arguments, dict) or set(arguments) != required or type(arguments["region_id"]) is not int or not 0 <= arguments["region_id"] <= 2147483647 or arguments["tool"] not in ("Guard", "ClassTrainer", "QuestGiver") or arguments["phase"] not in ("open", "select"):
                raise RuntimeErrorDetail("invalid_args", "NPC native tool UI requires exact token, region, tool, phase and CharacterTypeID pair")
            if not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None or type(arguments["character_slot"]) is not int or type(arguments["character_type"]) is not int:
                raise RuntimeErrorDetail("invalid_args", "NPC native tool UI token or CharacterTypeID types invalid")
            slot, kind = arguments["character_slot"], arguments["character_type"]
            if (arguments["phase"] == "open" and (slot, kind) != (-1, -1)) or (arguments["phase"] == "select" and not (0 <= slot <= 2147483647 and 0 <= kind <= 2147483647)):
                raise RuntimeErrorDetail("invalid_args", "NPC native tool UI CharacterTypeID does not match open/select phase")
        if op == "native_npc_place_probe":
            required = {"token", "region_id", "tool", "character_slot", "character_type", "position"}
            if not isinstance(arguments, dict) or set(arguments) != required or type(arguments["region_id"]) is not int or not 0 <= arguments["region_id"] <= 2147483647 or arguments["tool"] not in ("Guard", "ClassTrainer", "QuestGiver"):
                raise RuntimeErrorDetail("invalid_args", "NPC place probe requires exact token, region, tool, CharacterTypeID and position")
            if not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None or any(type(arguments[key]) is not int or not 0 <= arguments[key] <= 2147483647 for key in ("character_slot", "character_type")):
                raise RuntimeErrorDetail("invalid_args", "NPC place probe token or CharacterTypeID invalid")
            point = arguments["position"]
            if not isinstance(point, list) or len(point) != 3 or any(type(value) not in (int, float) or abs(value) > 3.4028234663852886e38 or not math.isfinite(value) for value in point):
                raise RuntimeErrorDetail("invalid_args", "NPC place probe requires finite float32 position")
        if op == "native_npc_quest_edit_ui":
            validate_npc_quest_edit(arguments)
        if op == "native_npc_quest_list_ui":
            validate_npc_quest_list(arguments)
        if op == "native_npc_quest_target_ui":
            validate_npc_quest_target(arguments)
        if op in {"native_dungeon_config_ui", "read_dungeon_config_ui"}:
            validate_dungeon_config_ui(arguments, read=op == "read_dungeon_config_ui")
        if op == "native_npc_property_ui":
            validate_npc_property(arguments)
        if op in {"native_npc_remove_ui", "native_npc_move_ui"}:
            validate_npc_target(arguments, move=op == "native_npc_move_ui", monster=op in {"native_npc_move_ui", "native_npc_remove_ui"})
        if op == "native_npc_place_ui":
            required = {"token", "region_id", "tool", "character_slot", "character_type", "position"}
            elite = isinstance(arguments, dict) and arguments.get("tool") == "item_elitemonster"
            if elite:
                required.add("level")
                if type(arguments.get("level")) is not int or not 1 <= arguments["level"] <= 2147483647 or arguments.get("character_slot") != 1:
                    raise RuntimeErrorDetail("invalid_args", "Elite UI requires positive actual level and CharacterTypeID slot1")
            if not isinstance(arguments, dict) or set(arguments) != required or type(arguments["region_id"]) is not int or not 0 <= arguments["region_id"] <= 2147483647 or arguments["tool"] not in ("Guard", "ClassTrainer", "QuestGiver", "item_elitemonster"):
                raise RuntimeErrorDetail("invalid_args", "NPC place UI requires exact token, region, tool, CharacterTypeID and position")
            if not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None or any(type(arguments[key]) is not int or not 0 <= arguments[key] <= 2147483647 for key in ("character_slot", "character_type")):
                raise RuntimeErrorDetail("invalid_args", "NPC place UI token or CharacterTypeID invalid")
            point = arguments["position"]
            if not isinstance(point, list) or len(point) != 3 or any(type(value) not in (int, float) or abs(value) > 1000000.0 or not math.isfinite(value) for value in point):
                raise RuntimeErrorDetail("invalid_args", "NPC place UI requires finite bounded position")
        if op in {"read_region_preset", "native_region_preset_ui"}:
            expected = {"region_id"} if op == "read_region_preset" else {"region_id", "preset", "diagnostic_uuid"}
            if not isinstance(arguments, dict) or set(arguments) != expected or type(arguments["region_id"]) is not int or not 0 <= arguments["region_id"] <= 2147483647:
                raise RuntimeErrorDetail("invalid_args", "region operation requires exact nonnegative int32 region ID and UI fields")
            if op == "native_region_preset_ui":
                if not isinstance(arguments["diagnostic_uuid"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["diagnostic_uuid"]) is None or not isinstance(arguments["preset"], str) or not 1 <= len(arguments["preset"].encode("utf-8")) <= 256 or "\0" in arguments["preset"]:
                    raise RuntimeErrorDetail("invalid_args", "region UI requires canonical diagnostic UUID and bounded preset string")
        if op == "native_character_rename_ui":
            validate_character_rename_ui(arguments)
        if op == "native_character_edit_ui":
            validate_character_edit_ui(arguments)
        if op == "read_character_edit_ui":
            validate_character_edit_read(arguments)
        if op == "read_character_rename":
            validate_character_rename_read(arguments)
        if op == "native_price_ui":
            if not isinstance(arguments, dict) or set(arguments) != {"diagnostic_uuid", "purchase_fee", "subscription_fee"} or not isinstance(arguments["diagnostic_uuid"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["diagnostic_uuid"]) is None or any(type(arguments[key]) is not int or not 0 <= arguments[key] <= 2147483647 for key in ("purchase_fee", "subscription_fee")):
                raise RuntimeErrorDetail("invalid_args", "price native UI requires canonical diagnostic UUID and two nonnegative int32 prices")
        if op == "native_region_property_ui":
            required = {"diagnostic_uuid", "region_id", "kind", "name", "type", "hit"}
            if isinstance(arguments, dict) and arguments.get("kind") == "levels":
                required.add("levels")
            appearance_kinds={"tint","light","ambient","sky","particles","particle_color"}
            if isinstance(arguments, dict) and arguments.get("kind") in appearance_kinds:
                required.add("appearance_value")
            if not isinstance(arguments, dict) or set(arguments) != required or type(arguments["region_id"]) is not int or not 0 <= arguments["region_id"] <= 2147483647 or not isinstance(arguments["diagnostic_uuid"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["diagnostic_uuid"]) is None:
                raise RuntimeErrorDetail("invalid_args", "region property UI requires exact diagnostic UUID and region ID")
            kind, name, property_type, hit = (arguments[key] for key in ("kind", "name", "type", "hit"))
            if not isinstance(kind, str) or kind not in {"name", "type", "purchase", "levels", "sale", *appearance_kinds} or not isinstance(name, str) or type(property_type) is not int or not isinstance(hit, list) or len(hit) != 3 or any(type(v) not in (int, float) or not math.isfinite(v) or abs(v) > 1000000 for v in hit):
                raise RuntimeErrorDetail("invalid_args", "region property kind/name/type/hit outside bounded domain")
            if kind in appearance_kinds:
                values=arguments["appearance_value"];used=16 if kind=="sky" else 1 if kind=="particles" else 4
                if name!="" or property_type!=-1 or hit!=[0,0,0] or not isinstance(values,list) or len(values)!=16 or any(type(v) not in (int,float) or not math.isfinite(v) or abs(v)>3.4028234663852886e38 for v in values) or any(v!=0 for v in values[used:]):
                    raise RuntimeErrorDetail("invalid_args", "region appearance requires finite native coefficients and no unrelated fields")
                if kind=="particles" and not (0<=values[0]<=6 and values[0]==int(values[0])):
                    raise RuntimeErrorDetail("invalid_args", "region particle type outside native domain")
            if kind == "levels":
                levels = arguments["levels"]
                if name != "" or property_type != -1 or not isinstance(levels, list) or len(levels) != 2 or any(type(v) is not int or not 1 <= v <= 214748354 for v in levels) or levels[0] > levels[1]:
                    raise RuntimeErrorDetail("invalid_args", "region levels require ordered native integer pair, empty name and type -1")
            if (kind == "name" and not (1 <= len(name.encode("utf-8")) <= 256 and "\0" not in name and property_type == -1 and hit == [0, 0, 0])) or (kind == "type" and not (name == "" and property_type in (0, 1))) or (kind in {"purchase", "sale"} and not (name == "" and property_type == -1)):
                raise RuntimeErrorDetail("invalid_args", "region property arguments differ from requested kind")
        if op == "probe_remove_ui":
            if not isinstance(arguments, dict) or set(arguments) != {"diagnostic_uuid", "snapshot_uuid", "record_key"}:
                raise RuntimeErrorDetail("invalid_args", "UI Remove requires exact diagnostic UUID, snapshot UUID and record key")
            for key in ("diagnostic_uuid", "snapshot_uuid"):
                if not isinstance(arguments[key], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments[key]) is None:
                    raise RuntimeErrorDetail("invalid_args", "UI Remove requires canonical scalar UUIDs")
            record = arguments["record_key"]
            if not valid_building_key(record) and not valid_network_key(record):
                raise RuntimeErrorDetail("invalid_args", "UI Remove requires a canonical Building, Scenery or Network key")
        if op == "request_snapshot_remove":
            if not isinstance(arguments, dict) or set(arguments) != {"action_uuid", "record_key"} or not isinstance(arguments["action_uuid"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["action_uuid"]) is None:
                raise RuntimeErrorDetail("invalid_args", "typed remove requires exact canonical action_uuid and record_key")
            record = arguments["record_key"]
            if not valid_building_key(record):
                raise RuntimeErrorDetail("invalid_args", "typed remove requires canonical SHA256 linked or saved root-inline building key")
        if op == "retire_snapshot_remove":
            if not isinstance(arguments, dict) or set(arguments) != {"token"} or not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None:
                raise RuntimeErrorDetail("invalid_args", "remove retirement requires exact canonical operation token")
        if op == "read_remove_bindings" and (not isinstance(arguments, dict) or arguments):
            raise RuntimeErrorDetail("invalid_args", "read_remove_bindings requires empty arguments")
        if op == "probe_remove_snapshot_building":
            required = {"token", "snapshot_id", "record_key", "expected_world_epoch", "expected_world_generation"}
            if not isinstance(arguments, dict) or set(arguments) != required:
                raise RuntimeErrorDetail("invalid_args", "snapshot remove requires exact token, snapshot, record and world binding")
            for key in ("token", "snapshot_id"):
                if not isinstance(arguments[key], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments[key]) is None:
                    raise RuntimeErrorDetail("invalid_args", f"{key} requires a canonical lowercase UUID")
            if not isinstance(arguments["record_key"], str):
                raise RuntimeErrorDetail("invalid_args", "record_key requires a string")
            try:
                record_bytes = len(arguments["record_key"].encode("utf-8"))
            except UnicodeEncodeError as exc:
                raise RuntimeErrorDetail("invalid_args", "record_key requires valid UTF-8") from exc
            if not 1 <= record_bytes <= 4096:
                raise RuntimeErrorDetail("invalid_args", "record_key requires 1 through 4096 UTF-8 bytes")
            for key in ("expected_world_epoch", "expected_world_generation"):
                if type(arguments[key]) is not int or not 1 <= arguments[key] <= 18446744073709551615:
                    raise RuntimeErrorDetail("invalid_args", f"{key} requires integer 1 through 18446744073709551615")
        if op == "read_native_free" and (not isinstance(arguments, dict) or arguments):
            raise RuntimeErrorDetail("invalid_args", "read_native_free requires empty arguments")
        if op == "probe_native_holder":
            if not isinstance(arguments, dict) or set(arguments) != {"token", "kind", "expected_world_epoch", "expected_world_generation"}:
                raise RuntimeErrorDetail("invalid_args", "probe_native_holder requires token, kind and expected world epoch/generation")
            if not isinstance(arguments["token"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["token"]) is None:
                raise RuntimeErrorDetail("invalid_args", "token requires a canonical lowercase UUID")
            if not isinstance(arguments["kind"], str) or arguments["kind"] not in {"prop", "building"}:
                raise RuntimeErrorDetail("invalid_args", "kind requires prop or building")
            for key in ("expected_world_epoch", "expected_world_generation"):
                if type(arguments[key]) is not int or not 1 <= arguments[key] <= 18446744073709551615:
                    raise RuntimeErrorDetail("invalid_args", f"{key} requires integer 1 through 18446744073709551615")
        if op in {"request_building_place","request_building_move"}:
            fields={"action_uuid","building"}|({"record_key"} if op=="request_building_move" else set())
            if not isinstance(arguments, dict) or set(arguments) != fields:
                raise RuntimeErrorDetail("invalid_args", "building request requires action_uuid and full building state")
            if op=="request_building_move":
                key=arguments["record_key"]
                if not valid_building_key(key):
                    raise RuntimeErrorDetail("invalid_args","Move requires a current shared building key")
            if not isinstance(arguments["action_uuid"], str) or re.fullmatch(TIME_TOKEN_PATTERN, arguments["action_uuid"]) is None:
                raise RuntimeErrorDetail("invalid_args", "building action requires canonical lowercase UUID")
            building = arguments["building"]
            if not isinstance(building, dict) or set(building) != set(BUILDING_STATE["required"]):
                raise RuntimeErrorDetail("invalid_args", "building state requires all eight typed fields")
            for key in ("type", "variant"):
                value = building[key]
                if not isinstance(value, str) or not 1 <= len(value) <= 256 or "\0" in value:
                    raise RuntimeErrorDetail("invalid_args", f"building {key} requires a bounded catalog identifier")
            if building["alive"] is not True:
                raise RuntimeErrorDetail("invalid_args", "placement requires alive=true")
            values = [building[key] for key in ("x", "y", "z")]
            for key, count in (("rotation", 4), ("scale", 3)):
                if not isinstance(building[key], list) or len(building[key]) != count:
                    raise RuntimeErrorDetail("invalid_args", f"building {key} requires exactly {count} numbers")
                values.extend(building[key])
            if any(type(value) not in {int, float} or abs(value) > FLOAT32_MAX or not math.isfinite(value) for value in values):
                raise RuntimeErrorDetail("invalid_args", "building pose requires finite float32-range numbers")
        if op == "capture_local_snapshot" and (not isinstance(arguments, dict) or arguments):
            raise RuntimeErrorDetail("invalid_args", "capture_local_snapshot requires empty arguments")
        if op in {"read_time_acceptance", "native_time_key", "read_time_input", "native_time_button", "read_time_mouse"}:
            keys = set(OP_SCHEMAS[op].get("required", ()))
            if not isinstance(arguments, dict) or set(arguments) != keys:
                raise RuntimeErrorDetail("invalid_args", f"{op} requires exactly {', '.join(sorted(keys)) or 'empty arguments'}")
            if op != "read_time_acceptance":
                token = arguments["acceptance_token"]
                if not isinstance(token, str) or re.fullmatch(TIME_TOKEN_PATTERN, token) is None:
                    raise RuntimeErrorDetail("invalid_args", "acceptance_token requires a canonical lowercase UUID")
            if op == "native_time_key":
                if not isinstance(arguments["key"], str) or arguments["key"] not in TIME_KEYS:
                    raise RuntimeErrorDetail("invalid_args", "key requires SPACE, F1, F2, F3 or F4")
                for key in ("expected_epoch", "expected_revision", "expected_request_sequence"):
                    if type(arguments[key]) is not int or not 1 <= arguments[key] <= 18446744073709551615:
                        raise RuntimeErrorDetail("invalid_args", f"{key} requires integer 1 through 18446744073709551615")
            if op == "native_time_button":
                if not isinstance(arguments["snapshot_uuid"],str) or re.fullmatch(TIME_TOKEN_PATTERN,arguments["snapshot_uuid"]) is None:
                    raise RuntimeErrorDetail("invalid_args","snapshot_uuid requires a canonical lowercase UUID")
                if type(arguments["scale"]) not in {int,float} or arguments["scale"] not in {0,1,3,9}:
                    raise RuntimeErrorDetail("invalid_args","native time button scale requires0,1,3,9")
                for key in ("expected_epoch","expected_world_epoch","expected_generation","expected_revision","expected_request_sequence"):
                    if type(arguments[key]) is not int or not 1<=arguments[key]<=18446744073709551615:
                        raise RuntimeErrorDetail("invalid_args",f"{key} requires positive uint64")
        if op in {"read_network_state", "read_subscriber_candidates", "read_time_ui", "read_building_catalog"} and arguments:
            raise RuntimeErrorDetail("invalid_args", f"{op} requires empty arguments")
        if op in {"arm_subscriber_needs", "apply_subscriber_needs", "disarm_subscriber_needs"}:
            keys = set(OP_SCHEMAS[op]["required"])
            if not isinstance(arguments, dict) or set(arguments) != keys:
                raise RuntimeErrorDetail("invalid_args", f"{op} requires exactly {', '.join(sorted(keys))}")
            for key in keys & {"epoch", "revision", "serialized_id"}:
                minimum, maximum = (0, 2147483647) if key == "serialized_id" else (1, 18446744073709551615)
                if type(arguments[key]) is not int or not minimum <= arguments[key] <= maximum:
                    raise RuntimeErrorDetail("invalid_args", f"{key} requires integer {minimum} through {maximum}")
            for key in keys & {"expected_initial", "expected_before", "host_needs"}:
                values = arguments[key]
                if not isinstance(values, list) or len(values) != 7:
                    raise RuntimeErrorDetail("invalid_args", f"{key} requires seven finite numbers")
                for value in values:
                    if type(value) not in {int, float} or not -100 <= value <= 100 or not math.isfinite(value):
                        raise RuntimeErrorDetail("invalid_args", f"{key} requires values in [-100,100]")
        if op in {"read_subscriber_needs", "read_subscriber_weapon"}:
            allowed={"serialized_id", "include_motion", "include_animation"} if op=="read_subscriber_weapon" else {"serialized_id"}
            if not isinstance(arguments,dict) or "serialized_id" not in arguments or not set(arguments)<=allowed or type(arguments["serialized_id"]) is not int or not 0<=arguments["serialized_id"]<=2147483647 or ("include_motion" in arguments and type(arguments["include_motion"]) is not bool) or ("include_animation" in arguments and type(arguments["include_animation"]) is not bool):
                raise RuntimeErrorDetail("invalid_args","serialized_id requires integer 0 through 2147483647")
        if op in {"purchase_test_region", "focus_test_camera"}:
            keys = ("x", "z") if op == "purchase_test_region" else ("x", "y", "z", "distance")
            if not isinstance(arguments, dict) or set(arguments) != set(keys):
                raise RuntimeErrorDetail("invalid_args", f"{op} requires exactly {', '.join(keys)}")
            for key in keys:
                value = arguments[key]
                if not isinstance(value, (int, float)) or isinstance(value, bool):
                    raise RuntimeErrorDetail("invalid_args", f"{op} {key} must be a finite float32-range number")
                try:
                    valid = math.isfinite(value) and abs(value) <= FLOAT32_MAX
                except OverflowError as exc:
                    raise RuntimeErrorDetail("invalid_args", f"{op} {key} exceeds finite coordinate representation") from exc
                if not valid:
                    raise RuntimeErrorDetail("invalid_args", f"{op} {key} must be a finite float32-range number")
            if op == "focus_test_camera" and struct.unpack("f", struct.pack("f", arguments["distance"]))[0] <= 0:
                raise RuntimeErrorDetail("invalid_args", "Camera distance must remain positive in native float32 representation")
        if op in {"probe_building_placement", "place_test_building"}:
            if not isinstance(arguments, dict) or set(arguments) != {"type", "variant", "x", "z"}:
                raise RuntimeErrorDetail("invalid_args", "Placement requires exactly type, variant, x and z")
            for key in ("type", "variant"):
                if not isinstance(arguments[key], str) or not 1 <= len(arguments[key]) <= 256:
                    raise RuntimeErrorDetail("invalid_args", f"Placement {key} must be string of 1 through 256 characters")
            for key in ("x", "z"):
                value = arguments[key]
                if not isinstance(value, (int, float)) or isinstance(value, bool):
                    raise RuntimeErrorDetail("invalid_args", f"Placement {key} must be finite number")
                try:
                    finite = math.isfinite(value)
                except OverflowError as exc:
                    raise RuntimeErrorDetail("invalid_args", f"Placement {key} exceeds finite coordinate representation") from exc
                if not finite:
                    raise RuntimeErrorDetail("invalid_args", f"Placement {key} must be finite number")
        if op in {"dismiss_crash_report","exit_game"} and arguments:
            raise RuntimeErrorDetail("invalid_args", f"{op} requires empty arguments")
        if op == "set_time_scale" and (not isinstance(arguments.get("scale"), (int, float))
                                       or isinstance(arguments["scale"], bool)
                                       or not math.isfinite(arguments["scale"]) or arguments["scale"] < 0):
            raise RuntimeErrorDetail("invalid_args", "scale must be a finite nonnegative number")
        if op in {"load_snapshot", "execute_action"}:
            key = "snapshot_id" if op == "load_snapshot" else "action_id"
            value = arguments.get(key)
            if not isinstance(value, str) or len(value) > 128 or not re.fullmatch(r"[A-Za-z0-9_.-]+", value) or value in {".", ".."}:
                raise RuntimeErrorDetail("invalid_args", f"{key} must be a bounded opaque identifier without a path")
        return runtime.request(op, arguments)
    raise RuntimeErrorDetail("unknown_tool", f"Unknown tool {name}")


async def serve(runtime: Runtime) -> None:
    server = Server("mmorpg-tycoon2-local", version="0.1.0")
    previous_tools: set[str] = set()

    @server.list_tools()
    async def list_tools() -> list[types.Tool]:
        nonlocal previous_tools
        result = await anyio.to_thread.run_sync(partial(tools_for, runtime))
        previous_tools = {tool.name for tool in result}
        return result

    @server.call_tool()
    async def call_tool(name: str, arguments: dict) -> types.CallToolResult:
        nonlocal previous_tools
        try:
            result = await anyio.to_thread.run_sync(partial(dispatch, runtime, name, arguments))
            current_tools = {tool.name for tool in await anyio.to_thread.run_sync(partial(tools_for, runtime))}
            if current_tools != previous_tools:
                previous_tools = current_tools
                await server.request_context.session.send_tool_list_changed()
            return types.CallToolResult(content=[types.TextContent(type="text", text=json.dumps(result, ensure_ascii=False))],
                                        structuredContent=result, isError=False)
        except RuntimeErrorDetail as exc:
            LOG.error("Tool failed op=%s code=%s cause=%s", name, exc.code, exc)
            receipt = {"native_response": exc.native_response} if exc.native_response is not None else None
            return types.CallToolResult(content=[types.TextContent(type="text", text=f"{exc.code}: {exc}")],
                                        structuredContent=receipt, isError=True)
        except Exception as exc:
            LOG.exception("Tool failed op=%s cause=%s", name, exc)
            return types.CallToolResult(content=[types.TextContent(type="text", text=f"internal_error: {type(exc).__name__}: {exc}")], isError=True)

    async with stdio_server() as (read_stream, write_stream):
        await server.run(read_stream, write_stream,
                         server.create_initialization_options(notification_options=NotificationOptions(tools_changed=True)))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--instances-root", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=5.0)
    options = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s %(message)s", stream=sys.stderr)
    try:
        runtime = Runtime(options.instances_root, timeout=options.timeout)
        anyio.run(serve, runtime)
    except Exception as exc:
        LOG.exception("Server failed root=%s cause=%s", options.instances_root, exc)
        raise SystemExit(1) from exc


if __name__ == "__main__":
    main()
