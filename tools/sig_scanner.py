#!/usr/bin/env python3
"""Scan MT2.exe for string references and generate AOB signatures for hooking."""

import struct
import sys
from pathlib import Path

def read_pe(path: str):
    """Read PE file and return sections."""
    with open(path, 'rb') as f:
        data = f.read()

    # Parse PE headers
    dos_magic = struct.unpack_from('<H', data, 0)[0]
    assert dos_magic == 0x5A4D, "Not a valid PE file"

    pe_offset = struct.unpack_from('<I', data, 0x3C)[0]
    pe_magic = struct.unpack_from('<I', data, pe_offset)[0]
    assert pe_magic == 0x00004550, "Invalid PE signature"

    num_sections = struct.unpack_from('<H', data, pe_offset + 6)[0]
    opt_header_size = struct.unpack_from('<H', data, pe_offset + 20)[0]

    sections = {}
    section_offset = pe_offset + 24 + opt_header_size

    image_base = struct.unpack_from('<Q', data, pe_offset + 24 + 24)[0]

    for i in range(num_sections):
        off = section_offset + i * 40
        name = data[off:off+8].rstrip(b'\x00').decode('ascii')
        virt_size = struct.unpack_from('<I', data, off + 8)[0]
        virt_addr = struct.unpack_from('<I', data, off + 12)[0]
        raw_size = struct.unpack_from('<I', data, off + 16)[0]
        raw_offset = struct.unpack_from('<I', data, off + 20)[0]

        sections[name] = {
            'virt_addr': virt_addr,
            'virt_size': virt_size,
            'raw_offset': raw_offset,
            'raw_size': raw_size,
            'data': data[raw_offset:raw_offset + raw_size]
        }

    return data, sections, image_base

def find_string(sections, search: str) -> list:
    """Find string in .rdata section, return RVAs."""
    rdata = sections.get('.rdata')
    if not rdata:
        return []

    results = []
    search_bytes = search.encode('utf-8')
    data = rdata['data']
    offset = 0

    while True:
        idx = data.find(search_bytes, offset)
        if idx == -1:
            break
        rva = rdata['virt_addr'] + idx
        results.append(rva)
        offset = idx + 1

    return results

def find_lea_references(sections, target_rva: int) -> list:
    """Find LEA instructions referencing the target RVA in .text."""
    text = sections.get('.text')
    if not text:
        return []

    results = []
    data = text['data']

    for i in range(len(data) - 7):
        # Check for LEA with RIP-relative: 48 8D XX or 4C 8D XX
        if data[i] in (0x48, 0x4C) and data[i+1] == 0x8D:
            modrm = data[i+2]
            if (modrm & 0xC7) == 0x05:  # [rip + disp32]
                disp = struct.unpack_from('<i', data, i + 3)[0]
                instr_rva = text['virt_addr'] + i
                instr_end_rva = instr_rva + 7
                ref_rva = instr_end_rva + disp

                if ref_rva == target_rva:
                    results.append(instr_rva)

    return results

def generate_signature(sections, rva: int, length: int = 16) -> str:
    """Generate AOB signature from function prologue."""
    text = sections.get('.text')
    if not text:
        return ""

    offset = rva - text['virt_addr']
    if offset < 0 or offset + length > len(text['data']):
        return ""

    # Walk back to find function start
    for back in range(0, 0x200):
        pos = offset - back
        if pos < 0:
            break
        byte = text['data'][pos]
        # Check for common MinGW x64 prologues
        if byte == 0x55 and text['data'][pos+1] == 0x48:  # push rbp; ...
            offset = pos
            break
        if pos > 0 and text['data'][pos-1] in (0xCC, 0xC3):  # after int3/ret
            if byte in (0x48, 0x55, 0x41, 0x53):
                offset = pos
                break

    sig_bytes = text['data'][offset:offset + length]
    return ' '.join(f'{b:02X}' for b in sig_bytes)

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <MT2.exe> <search_string>")
        print(f"Example: {sys.argv[0]} MT2.exe \"==> EDIT BEGINS:\"")
        sys.exit(1)

    exe_path = sys.argv[1]
    search = sys.argv[2]

    print(f"Scanning {exe_path} for \"{search}\"...")
    data, sections, image_base = read_pe(exe_path)

    rvas = find_string(sections, search)
    if not rvas:
        print("String not found in .rdata")
        sys.exit(1)

    for str_rva in rvas:
        print(f"\nString at RVA 0x{str_rva:08X} (VA 0x{image_base + str_rva:016X})")

        lea_refs = find_lea_references(sections, str_rva)
        if not lea_refs:
            print("  No LEA references found")
            continue

        for lea_rva in lea_refs:
            print(f"  LEA at RVA 0x{lea_rva:08X} (VA 0x{image_base + lea_rva:016X})")
            sig = generate_signature(sections, lea_rva)
            if sig:
                print(f"  Signature: {sig}")

if __name__ == "__main__":
    main()
