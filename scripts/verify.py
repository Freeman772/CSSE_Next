#!/usr/bin/env python3
"""Check the standalone build inputs and, optionally, a compiled x86 PE DLL.

Uses only Python's standard library. This does not replace a Windows build.
"""

import argparse
import json
from pathlib import Path
import re
import struct
import sys
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
SOURCE_ITEM_TYPES = {"ClCompile", "ClInclude", "ResourceCompile", "Image", "None"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_sources():
    counts = {}
    for relative in ("CSSE/CSSE.vcxproj", "SharedSE/SharedSE.vcxitems"):
        path = ROOT / relative
        tree = ET.parse(path)
        compiled = []
        for element in tree.iter():
            kind = element.tag.rsplit("}", 1)[-1]
            name = element.get("Include")
            if kind not in SOURCE_ITEM_TYPES or name is None:
                continue
            name = name.replace("$(MSBuildThisFileDirectory)", "").replace("\\", "/")
            require("$(" not in name, f"Unresolved source item in {relative}: {name}")
            source = (path.parent / name).resolve()
            require(source.is_relative_to(ROOT), f"Source outside repository: {name}")
            require(source.is_file(), f"Missing {relative} input: {name}")
            if kind == "ClCompile":
                compiled.append(source)
        require(len(compiled) == len(set(compiled)), f"Duplicate compilation units in {relative}")
        require(set(compiled) == set(path.parent.glob("*.cpp")), f"Unlisted C++ sources in {relative}")
        counts[path.parent.name] = len(compiled)

    for path in list(ROOT.rglob("*.props")) + list(ROOT.rglob("*.targets")):
        if "build" not in path.relative_to(ROOT).parts:
            ET.parse(path)

    project = ET.parse(ROOT / "CSSE/CSSE.vcxproj")
    for element in project.findall(".//m:Import", NS):
        path = element.get("Project", "")
        if "$(" not in path and not path.endswith("CSSE.User.props"):
            require((ROOT / "CSSE" / path.replace("\\", "/")).is_file(), f"Missing project import: {path}")
    solution = (ROOT / "CSSE.sln").read_text(encoding="utf-8-sig")
    projects = re.findall(r'^Project\("[^\"]+"\) = "([^\"]+)", "([^\"]+)"', solution, re.M)
    require(projects == [("CSSE", "CSSE\\CSSE.vcxproj")], "Solution must build only CSSE")
    require(not project.findall(".//m:ProjectReference", NS), "External project dependencies found")
    for forbidden in ("MWSE", "MWSE-Update", "MWSE-Tests", "packages", "deps/boost", "deps/rubic0n", "deps/sol", "deps/yaml-cpp", "deps/lpeg", ".gitmodules"):
        require(not (ROOT / forbidden).exists(), f"Unneeded dependency included: {forbidden}")
    for file in ("deps/fmt/include/fmt/format.h", "deps/toml11/include/toml.hpp", "deps/DirectX8/include/d3d8.h", "LICENSE.txt"):
        require((ROOT / file).is_file(), f"Missing dependency/license: {file}")

    # Resource filenames are resolved case-insensitively by the Windows SDK.
    resource = (ROOT / "CSSE/CSSE.rc").read_text(encoding="utf-16")
    resource += (ROOT / "CSSE/res/CSSE.rc2").read_text(encoding="utf-16")
    resource += (ROOT / "CSSE/res/icons.rc2").read_text(encoding="utf-8")
    files = {p.relative_to(ROOT / "CSSE").as_posix().lower() for p in (ROOT / "CSSE").rglob("*") if p.is_file()}
    for name in re.findall(r'^\s*(?:\w+|"[^\"]+")\s+(?:BITMAP|ICON|MANIFEST|RCDATA|24)\s+"([^\"]+)"', resource, re.M):
        require(name.replace("\\\\", "/").replace("\\", "/").lower() in files, f"Missing resource: {name}")
    icons = json.loads((ROOT / "CSSE/res/icons/catalog.json").read_text())
    for icon in icons:
        name = f"CSSE_ICON_{icon['theme'].upper()}_{icon['name'].upper()}"
        require('"' + name + '" RCDATA' in resource, f"Icon is not linked into the DLL: {name}")
        path = ROOT / "CSSE/res/icons" / icon['theme'] / (icon['name'] + '.png')
        data = path.read_bytes()
        require(data[:8] == b'\x89PNG\r\n\x1a\n' and struct.unpack_from('>II', data, 16) == (icon['width'], icon['height']), f"Invalid icon dimensions: {path}")
    require({icon['theme'] for icon in icons} == {'dark', 'light'}, 'Both theme resource sets must be present')
    require(not (ROOT / 'package/Data Files').exists(), 'Runtime icons must be embedded, not packaged as loose Data Files')
    configurations = [e.get('Include') for e in project.findall('.//m:ProjectConfiguration', NS)]
    require(configurations == ['Release|Win32'], 'Only Release must be exposed as a build configuration')
    print(f"Source inputs OK: {counts['CSSE']} CSSE + {counts['SharedSE']} shared C++ units; {len(icons)} embedded icon resources; Release only.")


def verify_binary(path):
    data = path.read_bytes()
    require(data[:2] == b"MZ", "Output is not a PE file")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    require(data[pe:pe + 4] == b"PE\0\0", "Invalid PE signature")
    machine, section_count, _, _, _, optional_size, characteristics = struct.unpack_from("<HHIIIHH", data, pe + 4)
    require(machine == 0x14C, "CSSE.dll must target x86 (IMAGE_FILE_MACHINE_I386)")
    require(characteristics & 0x2000, "Output is not marked as a DLL")
    optional = pe + 24
    require(struct.unpack_from("<H", data, optional)[0] == 0x10B, "Expected PE32")
    section_table = optional + optional_size
    sections = [struct.unpack_from("<IIII", data, section_table + i * 40 + 8) for i in range(section_count)]

    def offset(rva):
        for virtual_size, virtual_address, raw_size, raw_address in sections:
            if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
                return raw_address + rva - virtual_address
        raise ValueError(f"Unmapped PE address: {rva:#x}")

    def dll_name(rva):
        start = offset(rva)
        end = data.index(b"\0", start)
        return data[start:end].decode("ascii").lower()

    imports = set()
    directories = struct.unpack_from("<I", data, optional + 92)[0]
    for index, delayed in ((1, False), (13, True)):
        if directories <= index:
            continue
        rva, _ = struct.unpack_from("<II", data, optional + 96 + index * 8)
        if not rva:
            continue
        cursor = offset(rva)
        size = 32 if delayed else 20
        while any(data[cursor:cursor + size]):
            name_rva = struct.unpack_from("<I", data, cursor + (4 if delayed else 12))[0]
            if delayed and not (struct.unpack_from("<I", data, cursor)[0] & 1):
                name_rva -= struct.unpack_from("<I", data, optional + 28)[0]
            imports.add(dll_name(name_rva))
            cursor += size
    forbidden = sorted(name for name in imports if name.startswith(("mwse", "lua", "luajit", "yaml", "boost", "mfc", "msvcp", "msvcr", "vcruntime", "api-ms-win-crt-")) or name in {"mscoree.dll", "ucrtbase.dll"})
    require(not forbidden, f"Unexpected runtime dependencies: {', '.join(forbidden)}")
    print(f"PE OK: x86 DLL; {len(imports)} system imports; no MWSE/Lua/.NET or dynamic MFC/MSVC runtime imports.")
    print("Imports: " + ", ".join(sorted(imports)))

    # Check actual .rsrc payloads as well as their source RC declarations.
    resource_rva, _ = struct.unpack_from('<II', data, optional + 96 + 2 * 8)
    require(bool(resource_rva), 'The DLL contains no resources')
    resource_base = offset(resource_rva)
    embedded = {}

    def visit(relative, names):
        base = resource_base + relative
        named, numbered = struct.unpack_from('<HH', data, base + 12)
        for i in range(named + numbered):
            name, target = struct.unpack_from('<II', data, base + 16 + i * 8)
            if name & 0x80000000:
                pos = resource_base + (name & 0x7fffffff)
                length = struct.unpack_from('<H', data, pos)[0]
                name = data[pos+2:pos+2+length*2].decode('utf-16le')
            path = names + [name]
            if target & 0x80000000:
                require(len(path) <= 3, 'Invalid resource-directory nesting')
                visit(target & 0x7fffffff, path)
            elif len(path) == 3 and path[0] == 10 and isinstance(path[1], str) and path[1].startswith('CSSE_ICON_'):
                rva, size = struct.unpack_from('<II', data, resource_base + target)
                embedded[path[1]] = data[offset(rva):offset(rva)+size]

    visit(0, [])
    catalog = json.loads((ROOT / 'CSSE/res/icons/catalog.json').read_text())
    for icon in catalog:
        name = f"CSSE_ICON_{icon['theme'].upper()}_{icon['name'].upper()}"
        original = (ROOT / 'CSSE/res/icons' / icon['theme'] / (icon['name'] + '.png')).read_bytes()
        require(embedded.get(name) == original, f'DLL is missing the expected embedded PNG: {name}')
    print(f'Resources OK: all {len(catalog)} PNGs are embedded in CSSE.dll with matching bytes.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, help="Also verify the compiled CSSE.dll")
    args = parser.parse_args()
    try:
        verify_sources()
        if args.binary:
            verify_binary(args.binary)
    except (OSError, ValueError, ET.ParseError, struct.error) as error:
        print(f"Verification failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
