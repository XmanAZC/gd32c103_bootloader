#!/usr/bin/env python3
import argparse
import json
import re
from pathlib import Path


TYPE_MAP = {
    "u8": ("uint8_t", 1),
    "u16": ("uint16_t", 2),
    "u32": ("uint32_t", 4),
    "u64": ("uint64_t", 8),
    "i8": ("int8_t", 1),
    "i16": ("int16_t", 2),
    "i32": ("int32_t", 4),
    "i64": ("int64_t", 8),
    "bool": ("bool", 1),
    "float": ("float", 4),
    "f32": ("float", 4),
    "double": ("double", 8),
}


def to_snake(name: str) -> str:
    if "_" in name:
        return name.lower()
    s1 = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
    s2 = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s1)
    return s2.lower()


def to_upper_snake(name: str) -> str:
    return to_snake(name).upper()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def load_all(def_path: Path) -> dict:
    data = {"enums": [], "components": [], "messages": []}
    root = load_json(def_path)
    base_dir = def_path.parent

    for key in ("enums", "components", "messages"):
        data[key].extend(root.get(key, []))

    for rel in root.get("imports", []):
        imp = load_json(base_dir / rel)
        for key in ("enums", "components", "messages"):
            data[key].extend(imp.get(key, []))

    return data


def parse_type(type_name: str, enum_map: dict) -> dict:
    array_match = re.match(r"^([A-Za-z0-9_]+)\[(\d+)\]$", type_name)
    if array_match:
        base = array_match.group(1)
        count = int(array_match.group(2))
        return {"kind": "array", "base": base, "count": count}
    if type_name == "bytes":
        return {"kind": "bytes"}
    if type_name in enum_map:
        return {"kind": "enum", "name": type_name}
    return {"kind": "scalar", "name": type_name}


def c_type_for(type_info: dict, enum_map: dict) -> str:
    if type_info["kind"] == "enum":
        return f"xlink_{to_snake(type_info['name'])}_t"
    name = type_info["name"]
    if name not in TYPE_MAP:
        raise ValueError(f"Unknown type: {name}")
    return TYPE_MAP[name][0]


def size_of(type_info: dict, enum_map: dict) -> int:
    if type_info["kind"] == "enum":
        return 1
    name = type_info["name"]
    if name not in TYPE_MAP:
        raise ValueError(f"Unknown type: {name}")
    return TYPE_MAP[name][1]


def collect_enum_usage(fields, enum_map):
    used = set()
    for field in fields:
        info = parse_type(field["type"], enum_map)
        if info["kind"] == "enum":
            used.add(info["name"])
        elif info["kind"] == "array" and info["base"] in enum_map:
            used.add(info["base"])
    return used


def generate_header(component, messages_map, enum_map) -> str:
    comp_name = component["name"]
    comp_id = component["id"]
    comp_snake = to_snake(comp_name)
    comp_upper = to_upper_snake(comp_name)

    msg_names = component.get("messages", [])
    msg_ids = {name: idx for idx, name in enumerate(msg_names)}

    for name in msg_names:
        if name not in messages_map:
            raise ValueError(f"Message '{name}' not found for component '{comp_name}'")

    enums_used = set()
    for name in msg_names:
        enums_used |= collect_enum_usage(messages_map[name].get("fields", []), enum_map)

    lines = []
    guard = f"XLINK_{comp_upper}_H"
    lines.append("#pragma once")
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append('#include "../xlink.h"')
    lines.append("#include <stdbool.h>")
    lines.append("#include <stdint.h>")
    lines.append("#include <string.h>")
    lines.append("")
    lines.append(f"#define XLINK_COMP_ID_{comp_upper} {comp_id}")
    lines.append("")

    for msg_name, msg_id in msg_ids.items():
        msg_upper = to_upper_snake(msg_name)
        lines.append(f"#define XLINK_{comp_upper}_MSG_ID_{msg_upper} {msg_id}")
    lines.append("")

    for enum_name in sorted(enums_used):
        enum_def = enum_map[enum_name]
        enum_snake = to_snake(enum_name)
        lines.append(f"typedef uint8_t xlink_{enum_snake}_t;")
        for value in enum_def.get("values", []):
            val_name = to_upper_snake(value["name"])
            lines.append(f"#define XLINK_{to_upper_snake(enum_name)}_{val_name} {value['value']}")
        lines.append("")

    for msg_name in msg_names:
        msg_def = messages_map[msg_name]
        msg_snake = to_snake(msg_name)
        msg_upper = to_upper_snake(msg_name)
        type_name = f"xlink_{comp_snake}_{msg_snake}_t"

        fields = msg_def.get("fields", [])
        bytes_field = None
        fixed_size = 0
        for field in fields:
            info = parse_type(field["type"], enum_map)
            if info["kind"] == "bytes":
                if bytes_field is not None:
                    raise ValueError(f"Multiple bytes fields in {msg_name}")
                bytes_field = field
                fixed_size += 1
                continue
            if info["kind"] == "array":
                if info["base"] in enum_map:
                    item_size = 1
                else:
                    item_size = size_of({"kind": "scalar", "name": info["base"]}, enum_map)
                fixed_size += item_size * info["count"]
            else:
                fixed_size += size_of(info, enum_map)

        if bytes_field is not None and fields[-1]["name"] != bytes_field["name"]:
            raise ValueError(f"bytes field must be last in {msg_name}")

        if bytes_field is not None:
            bytes_name = bytes_field["name"]
            max_len = f"XLINK_{comp_upper}_{msg_upper}_{to_upper_snake(bytes_name)}_MAX_LEN"
            lines.append(f"#define {max_len} (XLINK_MAX_PAYLOAD - {fixed_size}u)")
            lines.append("")

        lines.append(f"typedef xlink_packed(struct {type_name}_def")
        lines.append("{")
        for field in fields:
            info = parse_type(field["type"], enum_map)
            field_name = field["name"]
            if info["kind"] == "bytes":
                max_len = f"XLINK_{comp_upper}_{msg_upper}_{to_upper_snake(field_name)}_MAX_LEN"
                lines.append(f"    uint8_t {field_name}_len;")
                lines.append(f"    uint8_t {field_name}[{max_len}];")
                continue
            if info["kind"] == "array":
                if info["base"] in enum_map:
                    c_type = f"xlink_{to_snake(info['base'])}_t"
                else:
                    c_type = TYPE_MAP[info["base"]][0]
                lines.append(f"    {c_type} {field_name}[{info['count']}];")
                continue
            c_type = c_type_for(info, enum_map)
            lines.append(f"    {c_type} {field_name};")
        lines.append(f"}}) {type_name};")
        lines.append("")
        params = ["xlink_context_p context"]
        for field in fields:
            info = parse_type(field["type"], enum_map)
            fname = field["name"]
            if info["kind"] == "bytes":
                params.append(f"const uint8_t *{fname}")
                params.append(f"uint8_t {fname}_len")
            elif info["kind"] == "array":
                base = info["base"]
                if base in enum_map:
                    c_type = f"xlink_{to_snake(base)}_t"
                else:
                    c_type = TYPE_MAP[base][0]
                params.append(f"const {c_type} *{fname}")
            elif info["kind"] == "enum":
                params.append(f"xlink_{to_snake(info['name'])}_t {fname}")
            else:
                params.append(f"{TYPE_MAP[info['name']][0]} {fname}")

        lines.append(f"static inline int xlink_{comp_snake}_{msg_snake}_send({', '.join(params)})")
        lines.append("{")
        lines.append(f"    {type_name} msg;")
        if bytes_field is not None:
            bytes_name = bytes_field["name"]
            lines.append(f"    if ({bytes_name}_len > 0u && {bytes_name} == NULL)")
            lines.append("    {")
            lines.append("        return -1;")
            lines.append("    }")
            lines.append(f"    if ({bytes_name}_len > {max_len})")
            lines.append("    {")
            lines.append("        return -1;")
            lines.append("    }")

        for field in fields:
            info = parse_type(field["type"], enum_map)
            fname = field["name"]
            if info["kind"] == "bytes":
                lines.append(f"    msg.{fname}_len = {fname}_len;")
                lines.append(f"    memcpy(msg.{fname}, {fname}, {fname}_len);")
                continue
            if info["kind"] == "array":
                count = info["count"]
                lines.append(f"    if ({fname} == NULL)")
                lines.append("    {")
                lines.append("        return -1;")
                lines.append("    }")
                lines.append(f"    memcpy(msg.{fname}, {fname}, sizeof(msg.{fname}[0]) * {count}u);")
                continue
            lines.append(f"    msg.{fname} = {fname};")

        if bytes_field is not None:
            bytes_name = bytes_field["name"]
            lines.append(f"    return xlink_send(context, XLINK_COMP_ID_{comp_upper}, XLINK_{comp_upper}_MSG_ID_{msg_upper}, (const uint8_t *)&msg, (uint8_t)({fixed_size}u + msg.{bytes_name}_len));")
        else:
            lines.append(f"    return xlink_send(context, XLINK_COMP_ID_{comp_upper}, XLINK_{comp_upper}_MSG_ID_{msg_upper}, (const uint8_t *)&msg, (uint8_t)sizeof(msg));")
        lines.append("}")
        lines.append("")

    lines.append(f"#endif // {guard}")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate XLINK message headers")
    parser.add_argument("-i", "--input", required=True, help="Path to demo.json")
    parser.add_argument("-o", "--output", required=True, help="Output directory for headers")
    args = parser.parse_args()

    data = load_all(Path(args.input))
    enum_map = {e["name"]: e for e in data["enums"]}
    messages_map = {m["name"]: m for m in data["messages"]}

    comp_ids = set()
    for comp in data["components"]:
        if comp["id"] in comp_ids:
            raise ValueError(f"Duplicate component id: {comp['id']}")
        comp_ids.add(comp["id"])

    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)

    for comp in data["components"]:
        comp_snake = to_snake(comp["name"])
        header_name = f"xlink_{comp_snake}.h"
        header_path = out_dir / header_name
        header_text = generate_header(comp, messages_map, enum_map)
        header_path.write_text(header_text, encoding="utf-8")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())