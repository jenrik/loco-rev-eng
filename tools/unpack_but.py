#!/usr/bin/env python3
"""
Lego Loco .but / packed asset (flags=0x1) Huffman decompressor.
Reverse-engineered from FUN_0045c830 at 0x0045c830 in loco.exe.

Packed format layout:
  [0x000]  uint32_le  uncompressed_size   byte count of output
  [0x004]  uint32_le  tree_root           starting node index for Huffman tree
  [0x008]  uint16[]   tree_table          Huffman node table (2040 bytes = 1020 entries)
  [0x800]  uint32[]   bit_stream          Huffman-compressed payload (32-bit words, LSB-first)

The tree table stores uint16 node values. Starting at tree_root, traverse by reading
one bit at a time: node = table[node * 2 + bit]. When node < 0x100, it's a leaf byte.

Usage:
  python3 unpack_but.py <packed.but>
  python3 unpack_but.py --all <art-res-dir> <output-dir>
"""

import struct
import sys
import os

HEADER_SIZE = 8
TABLE_SIZE  = 2040   # bytes = 1020 uint16 entries (0x008..0x7FF)
STREAM_OFF  = 0x808  # byte offset where bit stream starts


def decompress(data: bytes) -> bytes:
    """Decompress Huffman-packed data. Mirrors FUN_0045c830 exactly."""
    if len(data) < HEADER_SIZE + TABLE_SIZE:
        raise ValueError(f"Data too short: {len(data)} < {HEADER_SIZE + TABLE_SIZE}")

    uncompressed_size = struct.unpack_from('<I', data, 0)[0]
    tree_root         = struct.unpack_from('<I', data, 4)[0]

    out = bytearray(uncompressed_size)
    out_pos = 0

    # Bit stream state (mirrors: uVar3 = param_1[0x202], puVar7 = param_1 + 0x203)
    stream_word_offset = STREAM_OFF
    if stream_word_offset + 4 > len(data):
        raise ValueError("Bit stream offset past end of data")

    bits = struct.unpack_from('<I', data, stream_word_offset)[0]
    stream_word_offset += 4
    bits_left = 32

    for _ in range(uncompressed_size):
        node = tree_root

        # Traverse Huffman tree until leaf (node < 0x100)
        while node > 0xff:
            # Read one bit
            bit = bits & 1
            bits >>= 1
            bits_left -= 1

            if bits_left == 0:
                if stream_word_offset + 4 <= len(data):
                    bits = struct.unpack_from('<I', data, stream_word_offset)[0]
                    stream_word_offset += 4
                else:
                    bits = 0
                bits_left = 32

            # Navigate tree: iVar6 = (iVar6 * 2 + bit) * 2
            # then read uint16 at (param_1 + iVar6 + 8)
            idx = (node * 2 + bit) * 2
            table_byte_off = idx + 8
            if table_byte_off + 2 > len(data):
                raise ValueError(f"Table access out of bounds at offset {table_byte_off}")
            node = struct.unpack_from('<H', data, table_byte_off)[0]

        out[out_pos] = node
        out_pos += 1

    return bytes(out)


def main():
    import argparse

    parser = argparse.ArgumentParser(description='Decompress Lego Loco packed (.but) assets')
    parser.add_argument('input', nargs='?', help='Packed .but file')
    parser.add_argument('output', nargs='?', help='Output file (default: input.raw)')
    parser.add_argument('--all', metavar='ART_RES', help='Decompress all flags=0x1 assets from RFD')
    parser.add_argument('--out-dir', default='./unpacked', help='Output dir for --all mode')
    args = parser.parse_args()

    if args.all:
        # Decompress all packed assets from the RFH/RFD archive
        import struct

        rfh_path = os.path.join(args.all, 'resource.RFH')
        rfd_path = os.path.join(args.all, 'resource.RFD')

        with open(rfh_path, 'rb') as f:
            rfh = f.read()

        entries = []
        off = rfd_off = 0
        while off < len(rfh) - 4:
            name_len = struct.unpack_from('<I', rfh, off)[0]
            if name_len == 0 or name_len > 512:
                break
            off += 4
            name = rfh[off:off+name_len-1].decode('ascii', errors='replace')
            off += name_len
            rfd_size, flags = struct.unpack_from('<II', rfh, off)
            off += 8
            entries.append((name, rfd_off, rfd_size, flags))
            rfd_off += rfd_size

        packed = [(n, o, s, f) for n, o, s, f in entries if f == 1]
        print(f"Found {len(packed)} packed (flags=0x1) assets")

        os.makedirs(args.out_dir, exist_ok=True)
        ok = err = 0

        with open(rfd_path, 'rb') as rfd:
            for name, off, size, flags in packed:
                rfd.seek(off)
                data = rfd.read(size)
                out_path = os.path.join(args.out_dir,
                                        name.replace('\\', '/').lower())
                os.makedirs(os.path.dirname(out_path), exist_ok=True)
                try:
                    unpacked = decompress(data)
                    with open(out_path + '.raw', 'wb') as f:
                        f.write(unpacked)
                    ok += 1
                    print(f"  OK  {name}: {size} -> {len(unpacked)} bytes")
                except Exception as e:
                    err += 1
                    print(f"  ERR {name}: {e}")

        print(f"\nDecompressed {ok}/{ok+err} assets to {args.out_dir}/")

    elif args.input:
        with open(args.input, 'rb') as f:
            data = f.read()

        print(f"Input:  {args.input} ({len(data)} bytes)")
        print(f"Header: uncompressed_size={struct.unpack_from('<I', data, 0)[0]}, "
              f"tree_root={struct.unpack_from('<I', data, 4)[0]}")

        unpacked = decompress(data)
        out_path = args.output or (args.input + '.raw')
        with open(out_path, 'wb') as f:
            f.write(unpacked)
        print(f"Output: {out_path} ({len(unpacked)} bytes)")
        print(f"Ratio:  {len(data)/len(unpacked):.2f}:1 -> {len(data)*100//len(unpacked)}% of original")

    else:
        parser.print_help()


if __name__ == '__main__':
    main()
