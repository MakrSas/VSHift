#!/usr/bin/env python3
"""Extract selected entries from a decrypted PS4 PUP component.

The input must be a user-provided PS4UPDATE*.PUP.dec. This helper performs
container expansion only; it does not decrypt an official PUP or any SELF.
"""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path


ENTRY = struct.Struct("<IIQQQ")
HEADER = struct.Struct("<IIHBBHHQHHI")

FILE_NAMES = {
    3: "wlan_firmware.bin",
    5: "secure_modules.bin",
    6: "system_fs_image.img",
    8: "eap_fs_image.img",
    9: "recovery_fs_image.img",
    11: "preinst_fs_image.img",
    12: "system_ex_fs_image.img",
    34: "torus2_firmware.bin",
    257: "eula.xml",
    512: "orbis_swu.self",
    514: "orbis_swu.self",
    3337: "cp_firmware.bin",
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


def parse_entries(path: Path) -> list[Entry]:
    size = path.stat().st_size
    with path.open("rb") as source:
        fixed = source.read(HEADER.size)
        if len(fixed) != HEADER.size:
            raise ValueError("truncated PS4 PUP header")
        magic, _u04, _u08, _flags, _u0b, header_size, _hash_size, file_size, count, _hash_count, _u1c = HEADER.unpack(fixed)
        if magic != 0x1D3D154F:
            raise ValueError("input is not a decrypted PS4 PUP component")
        if file_size != size:
            raise ValueError("PUP file-size field does not match the selected file")
        table_end = HEADER.size + count * ENTRY.size
        if table_end > header_size or table_end > size:
            raise ValueError("PS4 PUP entry table is outside the header")
        entries = [Entry(index, ENTRY.unpack(source.read(ENTRY.size))) for index in range(count)]
    for entry in entries:
        if entry.offset + entry.compressed_size > size:
            raise ValueError(f"entry {entry.index} extends outside the PUP")
    return entries


def linked_table(entries: list[Entry], target: Entry) -> Entry:
    for candidate in entries:
        if candidate.flags & 1 and candidate.ident == target.index:
            return candidate
    raise ValueError(f"missing block table for entry {target.index}")


def zlib_expand(data: bytes, expected: int) -> bytes:
    result = zlib.decompress(data)
    if len(result) < expected:
        raise ValueError(f"zlib stream ended at {len(result)} bytes; expected {expected}")
    return result[:expected]


def extract(path: Path, entries: list[Entry], target: Entry, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    with path.open("rb") as source, destination.open("w+b") as output:
        if not target.blocked:
            source.seek(target.offset)
            raw = source.read(target.compressed_size)
            output.write(zlib_expand(raw, target.uncompressed_size) if target.compressed else raw)
            output.truncate(target.uncompressed_size)
            return

        block_size = 1 << (((target.flags & 0xF000) >> 12) + 12)
        block_count = (target.uncompressed_size + block_size - 1) // block_size
        table = linked_table(entries, target)
        block_infos: list[tuple[int, int]] = []
        if target.compressed:
            with path.open("rb") as table_source:
                table_source.seek(table.offset)
                table_data = table_source.read(table.compressed_size)
            if table.compressed:
                table_data = zlib_expand(table_data, table.uncompressed_size)
            info_start = block_count * 32
            for index in range(block_count):
                block_infos.append(struct.unpack_from("<II", table_data, info_start + index * 8))

        compressed_remaining = target.compressed_size
        uncompressed_remaining = target.uncompressed_size
        tail_size = target.uncompressed_size % block_size or block_size
        last_index = block_count - 1
        source.seek(target.offset)

        for index in range(block_count):
            block_is_compressed = False
            block_info_size = 0
            if target.compressed:
                block_offset, block_info_size = block_infos[index]
                unpadded_size = (block_info_size & ~0xF) - (block_info_size & 0xF)
                read_size = block_size
                if unpadded_size != block_size:
                    read_size = block_info_size
                    if index != last_index or tail_size != block_info_size:
                        read_size &= ~0xF
                        block_is_compressed = True
                if block_offset:
                    source.seek(target.offset + block_offset)
            else:
                read_size = min(block_size, compressed_remaining)
                source.seek(target.offset + target.compressed_size - compressed_remaining)

            if block_is_compressed:
                compressed = source.read(read_size - (block_info_size & 0xF))
                expanded = zlib.decompress(compressed)
                write_size = min(block_size, uncompressed_remaining)
                output.seek(index * block_size)
                output.write(expanded[:write_size])
                uncompressed_read = write_size
            else:
                raw = source.read(read_size)
                output.seek(index * block_size)
                output.write(raw)
                uncompressed_read = min(read_size, uncompressed_remaining)

            compressed_remaining -= read_size
            uncompressed_remaining -= uncompressed_read

        if uncompressed_remaining != 0:
            raise ValueError(f"entry {target.index} ended {uncompressed_remaining} bytes short")
        output.truncate(target.uncompressed_size)


def output_name(entry: Entry) -> str:
    return FILE_NAMES.get(entry.ident, f"unknown_{entry.ident}.img")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("pup", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--entry", type=int, action="append", required=True)
    args = parser.parse_args()

    entries = parse_entries(args.pup)
    for index in args.entry:
        target = entries[index]
        if target.special:
            continue
        destination = args.output / output_name(target)
        print(f"extracting entry {index}: {destination}", flush=True)
        extract(args.pup, entries, target, destination)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
