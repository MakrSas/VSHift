#!/usr/bin/env python3
"""Extract entries from a previously decrypted PS5 PUP.

This is a local research helper. It does not decrypt a PUP and it does not
ship firmware data; it only expands a user-provided .PUP.dec file.
"""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path


ENTRY_STRUCT = struct.Struct("<IIQQQ")
FILE_NAMES = {
    0x001: "eula.xml",
    0x002: "updatemode.self",
    0x003: "emc_salina_a.bin",
    0x004: "mbr.bin",
    0x005: "kernel.bin",
    0x006: "unk_06.bin",
    0x007: "unk_07.bin",
    0x008: "unk_08.bin",
    0x009: "unk_09.bin",
    0x00A: "CP.bin",
    0x00B: "titania.bls",
    0x00C: "version_name.xml",
    0x00D: "emc_salina_b.bin",
    0x00E: "eap_kbl.bin",
    0x00F: "bd_firm_info.json",
    0x010: "emc_salina_c.bls",
    0x011: "floyd_salina_c.bls",
    0x012: "usb_pdc_salina_c.bls",
    0x014: "emc_salina_d.bls",
    0x015: "eap_kbl_2.bin",
    0x016: "font.zip",
    0x100: "ariel_sec_ldr_a.bin",
    0x101: "oberon_sec_ldr_a.bin",
    0x102: "oberon_sec_ldr_b.bin",
    0x103: "oberon_sec_ldr_c.bin",
    0x104: "oberon_sec_ldr_d.bin",
    0x105: "oberon_sec_ldr_e.bin",
    0x106: "oberon_sec_ldr_f.bin",
}

DEVICE_NAMES = {
    0x200: "dev/unk_512.bin",
    0x201: "dev/wlanbt.bin",
    0x202: "dev/unk_514.bin",
    0x203: "dev/ssd0.system_b",
    0x204: "dev/ssd0.system_ex_b",
    0x205: "dev/unk_517.bin",
    0x206: "dev/unk_518.bin",
    0x207: "dev/ssd0.preinst",
}


class Entry:
    __slots__ = ("index", "flags", "offset", "compressed_size", "uncompressed_size")

    def __init__(self, index: int, values: tuple[int, int, int, int, int]) -> None:
        self.index = index
        self.flags, _reserved, self.offset, self.compressed_size, self.uncompressed_size = values

    @property
    def ident(self) -> int:
        return self.flags >> 20

    @property
    def compressed(self) -> bool:
        return bool(self.flags & 8)

    @property
    def blocked(self) -> bool:
        return bool(self.flags & 0x800)

    @property
    def special(self) -> bool:
        return (self.flags & 0xF0000000) in (0xE0000000, 0xF0000000)


def read_entries(path: Path) -> list[Entry]:
    with path.open("rb") as stream:
        fixed = stream.read(0x20)
        if len(fixed) != 0x20:
            raise ValueError("truncated PUP header")
        magic, _u04, _u08, _flags, _u0b, header_size, _hash_size, file_size, count, _hash_count, _u1c = struct.unpack(
            "<IIHBBHHQHHI", fixed
        )
        if magic != 0xEEF51454:
            raise ValueError("input is not a decrypted PS5 PUP (.PUP.dec)")
        if file_size != path.stat().st_size:
            raise ValueError("PUP file-size field does not match the file")
        stream.seek(0x20)
        entries = [Entry(i, ENTRY_STRUCT.unpack(stream.read(ENTRY_STRUCT.size))) for i in range(count)]
        if 0x20 + count * ENTRY_STRUCT.size > header_size:
            raise ValueError("PUP entry table exceeds the header")
        return entries


def table_for(entries: list[Entry], entry: Entry) -> Entry:
    for candidate in entries:
        if candidate.flags & 1 and candidate.ident == entry.index:
            return candidate
    raise ValueError(f"missing block table for entry {entry.index}")


def zlib_expand(data: bytes, expected: int) -> bytes:
    expanded = zlib.decompress(data)
    if len(expanded) < expected:
        raise ValueError(f"zlib stream ended at {len(expanded)} bytes; expected {expected}")
    return expanded[:expected]


def extract_entry(path: Path, entries: list[Entry], entry: Entry, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    with path.open("rb") as source, output.open("w+b") as target:
        if not entry.blocked:
            source.seek(entry.offset)
            raw = source.read(entry.compressed_size)
            target.write(zlib_expand(raw, entry.uncompressed_size) if entry.compressed else raw)
            target.truncate(entry.uncompressed_size)
            return

        block_size = 1 << (((entry.flags & 0xF000) >> 12) + 12)
        block_count = (block_size + entry.uncompressed_size - 1) // block_size
        block_table = table_for(entries, entry)

        infos: list[tuple[int, int]] = []
        if entry.compressed:
            with path.open("rb") as table_source:
                table_source.seek(block_table.offset)
                table_data = table_source.read(block_table.compressed_size)
            if block_table.compressed:
                table_data = zlib_expand(table_data, block_table.uncompressed_size)
            info_start = 32 * block_count
            for index in range(block_count):
                offset, size = struct.unpack_from("<II", table_data, info_start + index * 8)
                infos.append((offset, size))

        remaining_compressed = entry.compressed_size
        remaining_uncompressed = entry.uncompressed_size
        tail_size = entry.uncompressed_size % block_size or block_size
        last_index = block_count - 1

        for index in range(block_count):
            block_is_compressed = False
            if entry.compressed:
                block_offset, block_info_size = infos[index]
                unpadded_size = (block_info_size & ~0xF) - (block_info_size & 0xF)
                read_size = block_size
                if unpadded_size != block_size:
                    read_size = block_info_size
                    if index != last_index or tail_size != block_info_size:
                        read_size &= ~0xF
                        block_is_compressed = True
                if block_offset:
                    source.seek(entry.offset + block_offset)
                else:
                    source.seek(entry.offset)
            else:
                read_size = min(block_size, remaining_compressed)
                source.seek(entry.offset + (entry.compressed_size - remaining_compressed))

            if block_is_compressed:
                compressed = source.read(read_size - (block_info_size & 0xF))
                expanded = zlib.decompress(compressed)
                write_size = min(block_size, remaining_uncompressed)
                target.seek(index * block_size)
                target.write(expanded[:write_size])
                remaining_uncompressed -= write_size
            else:
                raw = source.read(read_size)
                target.seek(index * block_size)
                target.write(raw)
                remaining_uncompressed -= min(read_size, remaining_uncompressed)

            remaining_compressed -= read_size

        target.truncate(entry.uncompressed_size)


def name_for(entries: list[Entry], entry: Entry, table_links: dict[int, int]) -> str:
    linked = table_links.get(entry.index)
    if linked is not None:
        return f"tables/{entry.ident}_for_{entries[linked].ident}.img"
    return FILE_NAMES.get(entry.ident, DEVICE_NAMES.get(entry.ident, f"unknown/{entry.ident}.img"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("pup", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--entry", type=int, action="append", help="PUP entry index; repeatable")
    args = parser.parse_args()

    entries = read_entries(args.pup)
    links = {candidate.index: entry.index for entry in entries if entry.blocked for candidate in entries if candidate.flags & 1 and candidate.ident == entry.index}
    selected = set(args.entry or [index for index, entry in enumerate(entries) if not entry.special])
    for index in sorted(selected):
        entry = entries[index]
        if entry.special:
            continue
        relative = name_for(entries, entry, links)
        destination = args.output / f"{index:02d}_{Path(relative).name}"
        print(f"extracting entry {index}: {relative} -> {destination}", flush=True)
        extract_entry(args.pup, entries, entry, destination)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
